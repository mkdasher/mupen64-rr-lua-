//#include "../config.h"
#include <assert.h>

#include "win/main_win.h"
#include "win/timers.h"
#include "win/features/Statusbar.hpp"
#include "win/features/Toolbar.hpp"

#include "../lua/LuaConsole.h"

#include "vcr.h"
#include "vcr_compress.h"
#include "vcr_resample.h"

//ffmpeg
#include "ffmpeg_capture/ffmpeg_capture.hpp"
#include <memory>

#include "plugin.hpp"
#include "rom.h"
#include "savestates.h"
#include "../memory/memory.h"

#include <filesystem>
#include <errno.h>
#include <limits.h>
#include <memory.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#ifdef _MSC_VER
#define snprintf	_snprintf
#define strcasecmp	_stricmp
#define strncasecmp	_strnicmp
#else
#include <unistd.h>
#endif
//#include <zlib.h>
#include <stdio.h>
#include <time.h>
#include <chrono>

#include <commctrl.h> // for SendMessage, SB_SETTEXT
#include <Windows.h> // for truncate functions
#include <../../winproject/resource.h> // for EMU_RESET
#include "win/Config.hpp" //config struct
#include "win/main_win.h" // mainHWND
#include <WinUser.h>

#include "win/Commandline.h"


#ifdef _DEBUG
#include "../r4300/macros.h"
#endif

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

// M64\0x1a
#define MUP_MAGIC (0x1a34364d)
#define MUP_VERSION (3)
#define MUP_HEADER_SIZE_OLD (512)
#define MUP_HEADER_SIZE (sizeof(t_movie_header))
#define MUP_HEADER_SIZE_CUR (vcr_movie_header.version <= 2 ? MUP_HEADER_SIZE_OLD : MUP_HEADER_SIZE)
#define MAX_AVI_SIZE 0x7B9ACA00

BOOL dontPlay = false;

#define BUFFER_GROWTH_SIZE (4096)

static const char* m_errCodeName[] =
{
	"Success",
	"Wrong Format",
	"Wrong Version",
	"File Not Found",
	"Not From This Movie",
	"Not From A Movie",
	"Invalid Frame",
	"Unknown Error"
};

e_task m_task = e_task::idle;

static char AVIFileName[PATH_MAX];
std::filesystem::path movie_path;
std::vector<BUTTONS> movie_inputs;

t_movie_header vcr_movie_header = {0};
static BOOL m_readOnly = FALSE;

int64_t vcr_current_sample = -1;
// should = length_samples when recording, and be < length_samples when playing
int m_currentVI = -1;

static int m_capture = 0; // capture movie
static int m_audioFreq = 33000; //0x30018;
static int m_audioBitrate = 16; // 16 bits
static long double m_videoFrame = 0;
static long double m_audioFrame = 0;
#define SOUND_BUF_SIZE 44100*2*2 // 44100=1s sample, soundbuffer capable of holding 4s future data in circular buffer

static char soundBuf[SOUND_BUF_SIZE];
static char soundBufEmpty[SOUND_BUF_SIZE];
static int soundBufPos = 0;
long lastSound = 0;
volatile BOOL captureFrameValid = FALSE;
int AVIIncrement = 0;
int titleLength;
char VCR_Lastpath[MAX_PATH];
bool is_restarting_flag = false;

bool captureWithFFmpeg = true;
std::unique_ptr<FFmpegManager> captureManager;
uint64_t screen_updates = 0;

static int restartPlayback();

char* strtrimext(char* myStr)
{
	char* retStr;
	char* lastExt;
	if (myStr == NULL) return NULL;
	if ((retStr = (char*)malloc(strlen(myStr) + 1)) == NULL) return NULL;
	strcpy(retStr, myStr);
	lastExt = strrchr(retStr, '.');
	if (lastExt != NULL)
		*lastExt = '\0';
	return retStr;
}

void printError(const char* str)
{
	MessageBox(NULL, str, "Error", MB_OK | MB_ICONERROR);
}


static void hardResetAndClearAllSaveData(bool clear)
{
	extern BOOL clear_sram_on_restart_mode;
	clear_sram_on_restart_mode = clear;
	continue_vcr_on_restart_mode = TRUE;
	if (clear)
		printf("Clearing save data...\n");
	else
		printf("Playing movie without clearing save data\n");
	SendMessage(mainHWND, WM_COMMAND, EMU_RESET, 0);
}

void apply_rom_info(t_movie_header* header)
{
	header->vis_per_second = static_cast<uint8_t>(get_vis_per_second(ROM_HEADER.Country_code));
	header->controller_flags = 0;
	header->num_controllers = 0;

	for (int i = 0; i < 4; ++i)
	{
		if (Controls[i].Plugin == controller_extension::mempak)
		{
			header->controller_flags |= CONTROLLER_X_MEMPAK(i);
		}
		if (Controls[i].Plugin == controller_extension::rumblepak)
		{
			header->controller_flags |= CONTROLLER_X_RUMBLE(i);
		}

		if (!Controls[i].Present)
		{
			continue;
		}
		header->controller_flags |= CONTROLLER_X_PRESENT(i);
		header->num_controllers++;
	}

	strncpy(header->rom_name, (const char*)ROM_HEADER.nom, 32);
	header->rom_crc1 = ROM_HEADER.CRC1;
	header->rom_country = ROM_HEADER.Country_code;
	memset(header->video_plugin_name, 0, std::size(header->video_plugin_name));
	memset(header->audio_plugin_name, 0, std::size(header->audio_plugin_name));
	memset(header->input_plugin_name, 0, std::size(header->input_plugin_name));
	memset(header->rsp_plugin_name, 0, std::size(header->rsp_plugin_name));
	strncpy(header->video_plugin_name, video_plugin->name.c_str(), 64);
	strncpy(header->input_plugin_name, input_plugin->name.c_str(), 64);
	strncpy(header->audio_plugin_name, audio_plugin->name.c_str(), 64);
	strncpy(header->rsp_plugin_name, rsp_plugin->name.c_str(), 64);
}


static int read_movie_header(FILE* file, t_movie_header* header)
{
	fseek(file, 0L, SEEK_SET);

	t_movie_header newHeader;
	memset(&newHeader, 0, sizeof(t_movie_header));

	if (fread(&newHeader, 1, MUP_HEADER_SIZE_OLD, file) != MUP_HEADER_SIZE_OLD)
		return WRONG_FORMAT;

	if (newHeader.magic != MUP_MAGIC)
		return WRONG_FORMAT;

	if (newHeader.version <= 0 || newHeader.version > MUP_VERSION)
		return WRONG_VERSION;

	if (newHeader.version == 1 || newHeader.version == 2)
	{
		// attempt to recover screwed-up plugin data caused by
		// version mishandling and format problems of first versions

#define isAlpha(x) (((x) >= 'A' && (x) <= 'Z') || ((x) >= 'a' && (x) <= 'z') || ((x) == '1'))
		int i;
		for (i = 0; i < 56 + 64; i++)
			if (isAlpha(newHeader.reserved_bytes[i])
				&& isAlpha(newHeader.reserved_bytes[i + 64])
				&& isAlpha(newHeader.reserved_bytes[i + 64 + 64])
				&& isAlpha(newHeader.reserved_bytes[i + 64 + 64 + 64]))
				break;
		if (i != 56 + 64)
		{
			memmove(newHeader.video_plugin_name, newHeader.reserved_bytes + i,
			        256);
		} else
		{
			for (i = 0; i < 56 + 64; i++)
				if (isAlpha(newHeader.reserved_bytes[i])
					&& isAlpha(newHeader.reserved_bytes[i + 64])
					&& isAlpha(newHeader.reserved_bytes[i + 64 + 64]))
					break;
			if (i != 56 + 64)
				memmove(newHeader.audio_plugin_name, newHeader.reserved_bytes + i,
				        256 - 64);
			else
			{
				for (i = 0; i < 56 + 64; i++)
					if (isAlpha(newHeader.reserved_bytes[i])
						&& isAlpha(newHeader.reserved_bytes[i + 64]))
						break;
				if (i != 56 + 64)
					memmove(newHeader.input_plugin_name,
					        newHeader.reserved_bytes + i, 256 - 64 - 64);
				else
				{
					for (i = 0; i < 56 + 64; i++)
						if (isAlpha(newHeader.reserved_bytes[i]))
							break;
					if (i != 56 + 64)
						memmove(newHeader.rsp_plugin_name,
						        newHeader.reserved_bytes + i,
						        256 - 64 - 64 - 64);
					else
						strncpy(newHeader.rsp_plugin_name, "(unknown)", 64);

					strncpy(newHeader.input_plugin_name, "(unknown)", 64);
				}
				strncpy(newHeader.audio_plugin_name, "(unknown)", 64);
			}
			strncpy(newHeader.video_plugin_name, "(unknown)", 64);
		}
		// attempt to convert old author and description to utf8
		strncpy(newHeader.author, newHeader.old_author_info, 48);
		strncpy(newHeader.description, newHeader.old_description, 80);
	}
	if (newHeader.version == 3)
	{
		// read rest of header
		if (fread((char*)(&newHeader) + MUP_HEADER_SIZE_OLD, 1,
		          MUP_HEADER_SIZE - MUP_HEADER_SIZE_OLD,
		          file) != MUP_HEADER_SIZE - MUP_HEADER_SIZE_OLD)
			return WRONG_FORMAT;
	}

	*header = newHeader;

	return SUCCESS;
}

t_movie_header VCR_getHeaderInfo(const char* filename)
{
	char buf[PATH_MAX];
	char temp_filename[PATH_MAX];
	t_movie_header tempHeader;
	memset(&tempHeader, 0, sizeof(t_movie_header));
	tempHeader.rom_country = -1;
	strcpy(tempHeader.rom_name, "(no ROM)");

	strncpy(temp_filename, filename, PATH_MAX);
	char* p = strrchr(temp_filename, '.');
	if (p)
	{
		if (!strcasecmp(p, ".m64") || !strcasecmp(p, ".st"))
			*p = '\0';
	}
	// open record file
	strncpy(buf, temp_filename, PATH_MAX);
	FILE* tempFile = fopen(buf, "rb+");
	if (tempFile == 0 && (tempFile = fopen(buf, "rb")) == 0)
	{
		strncat(buf, ".m64", 4);
		tempFile = fopen(buf, "rb+");
		if (tempFile == 0 && (tempFile = fopen(buf, "rb")) == 0)
		{
			fprintf(
				stderr,
				"[VCR]: Could not get header info of .m64 file\n\"%s\": %s\n",
				filename, strerror(errno));
			return tempHeader;
		}
	}

	read_movie_header(tempFile, &tempHeader);
	fclose(tempFile);
	return tempHeader;
}

void vcr_clear_save_data()
{
	{
		FILE* f = fopen(get_sram_path().string().c_str(), "wb");
		if (f)
		{
			extern unsigned char sram[0x8000];
			for (int i = 0; i < 0x8000; i++) sram[i] = 0;
			fwrite(sram, 1, 0x8000, f);
			fclose(f);
		}
	}
	{
		FILE* f = fopen(get_eeprom_path().string().c_str(), "wb");
		if (f)
		{
			extern unsigned char eeprom[0x8000];
			for (int i = 0; i < 0x800; i++) eeprom[i] = 0;
			fwrite(eeprom, 1, 0x800, f);
			fclose(f);
		}
	}
	{
		FILE* f = fopen(get_mempak_path().string().c_str(), "wb");
		if (f)
		{
			extern unsigned char mempack[4][0x8000];
			for (int j = 0; j < 4; j++)
			{
				for (int i = 0; i < 0x800; i++) mempack[j][i] = 0;
				fwrite(mempack[j], 1, 0x800, f);
			}
			fclose(f);
		}
	}
}


BOOL
VCR_isActive()
{
	return (m_task == e_task::recording || m_task == e_task::playback) ? TRUE : FALSE;
}

BOOL
VCR_isIdle()
{
	return (m_task == e_task::idle) ? TRUE : FALSE;
}

BOOL
VCR_isStarting()
{
	return (m_task == e_task::start_playback || m_task == e_task::start_playback_from_snapshot)
		       ? TRUE
		       : FALSE;
}

BOOL
VCR_isStartingAndJustRestarted()
{
	extern BOOL just_restarted_flag;
	if (m_task == e_task::start_playback && !continue_vcr_on_restart_mode &&
		just_restarted_flag)
	{
		just_restarted_flag = FALSE;
		vcr_current_sample = 0;
		m_currentVI = 0;
		m_task = e_task::playback;
		return TRUE;
	}

	return FALSE;
}

BOOL
VCR_isPlaying()
{
	return (m_task == e_task::playback) ? TRUE : FALSE;
}

BOOL
VCR_isRecording()
{
	return (m_task == e_task::recording) ? TRUE : FALSE;
}

BOOL
VCR_isCapturing()
{
	return m_capture ? TRUE : FALSE;
}

BOOL
VCR_getReadOnly()
{
	return m_readOnly;
}

void
VCR_setReadOnly(BOOL val)
{
	extern HWND mainHWND;
	if (m_readOnly != val)
		CheckMenuItem(GetMenu(mainHWND), EMU_VCRTOGGLEREADONLY,
		              MF_BYCOMMAND | (val ? MFS_CHECKED : MFS_UNCHECKED));
	m_readOnly = val;
}

bool VCR_isLooping()
{
	return Config.is_movie_loop_enabled;
}

bool VCR_isRestarting()
{
	return is_restarting_flag;
}

void VCR_setLoopMovie(bool val)
{
	extern HWND mainHWND;
	if (VCR_isLooping() != val)
		CheckMenuItem(GetMenu(mainHWND), ID_LOOP_MOVIE,
		              MF_BYCOMMAND | (val ? MFS_CHECKED : MFS_UNCHECKED));
	Config.is_movie_loop_enabled = val;
}

unsigned long VCR_getLengthVIs()
{
	return VCR_isActive() ? vcr_movie_header.length_vis : 0;
}

unsigned long VCR_getLengthSamples()
{
	return VCR_isActive() ? vcr_movie_header.length_samples : 0;
}

void VCR_setLengthVIs(unsigned long val)
{
	vcr_movie_header.length_vis = val;
}


extern BOOL continue_vcr_on_restart_mode;
extern BOOL just_restarted_flag;

void vcr_on_controller_poll(int index, BUTTONS* input)
{
	// if we aren't playing back a movie, our data source isn't movie
	if (!is_task_playback(m_task))
	{
		getKeys(index, input);
		last_controller_data[index] = *input;
		main_dispatcher_invoke([index] {
			AtInputLuaCallback(index);
		});

		// if lua requested a joypad change, we overwrite the data with lua's changed value for this cycle
		if (overwrite_controller_data[index])
		{
			*input = new_controller_data[index];
			last_controller_data[index] = *input;
			overwrite_controller_data[index] = false;
		}
	}

	if (m_task == e_task::idle)
		return;

	if (m_task == e_task::start_recording)
	{
		if (!continue_vcr_on_restart_mode)
		{
			if (just_restarted_flag)
			{
				just_restarted_flag = FALSE;
				vcr_current_sample = 0;
				m_currentVI = 0;
				m_task = e_task::recording;
				*input = {0};
				EnableMenuItem(GetMenu(mainHWND), ID_STOP_RECORD, MF_ENABLED);
			} else
			{
				printf("[VCR]: Starting recording...\n");
				hardResetAndClearAllSaveData(
					!(vcr_movie_header.startFlags & MOVIE_START_FROM_EEPROM));
			}
		}
	}

	if (m_task == e_task::start_recording_from_snapshot)
	{
		// TODO: maybe call st generation like normal and remove the "start_x" states
		if (savestates_job == e_st_job::none)
		{
			printf("[VCR]: Starting recording from Snapshot...\n");
			m_task = e_task::recording;
			*input = {0};
		}
	}

	if (m_task == e_task::start_recording_from_existing_snapshot)
	{
		// TODO: maybe call st generation like normal and remove the "start_x" states
		if (savestates_job == e_st_job::none)
		{
			printf("[VCR]: Starting recording from Existing Snapshot...\n");
			m_task = e_task::recording;
			*input = {0};
		}
	}


	if (m_task == e_task::start_playback)
	{
		if (!continue_vcr_on_restart_mode)
		{
			if (just_restarted_flag)
			{
				just_restarted_flag = FALSE;
				vcr_current_sample = 0;
				m_currentVI = 0;
				m_task = e_task::playback;
			} else
			{
				printf("[VCR]: Starting playback...\n");
				hardResetAndClearAllSaveData(
					!(vcr_movie_header.startFlags & MOVIE_START_FROM_EEPROM));
			}
		}
	}

	if (m_task == e_task::start_playback_from_snapshot)
	{
		// TODO: maybe call st generation like normal and remove the "start_x" states
		if (savestates_job == e_st_job::none)
		{
			if (!savestates_job_success)
			{
				m_task = e_task::idle;
				getKeys(index, input);
				return;
			}
			printf("[VCR]: Starting playback...\n");
			m_task = e_task::playback;
		}
	}

	if (m_task == e_task::recording)
	{
		extern bool scheduled_restart;
		if (scheduled_restart)
		{
			// reserved 1 and 2 pressed simultaneously = reset flag
			*input = {
				.Reserved1 = 1,
				.Reserved2 = 1,
			};
		}

		printf("Recording Input [%d] (len %d): %d %d\n", vcr_current_sample, movie_inputs.size(), input->Y_AXIS, input->X_AXIS);
		movie_inputs.push_back(*input);
		vcr_movie_header.length_samples++;
		vcr_current_sample++;

		if (scheduled_restart)
		{
			resetEmu();
			scheduled_restart = false;
		}
		return;
	}

	// our input source is movie, input plugin is overriden
	if (m_task == e_task::playback)
	{
		if (vcr_current_sample >= vcr_movie_header.length_samples)
		{
			vcr_stop_playback(false);
			commandline_on_movie_playback_stop();
			setKeys(index, {0});
			getKeys(index, input);
			return;
		}

		if (vcr_movie_header.controller_flags & CONTROLLER_X_PRESENT(index))
		{
			*input = movie_inputs[vcr_current_sample];
			setKeys(index, movie_inputs[vcr_current_sample]);

			//no readable code because 120 star tas can't get this right >:(
			if (input->Value == 0xC000)
			{
				continue_vcr_on_restart_mode = true;
				resetEmu();
			}

			last_controller_data[index] = *input;
			main_dispatcher_invoke([index] {
				AtInputLuaCallback(index);
			});

			// if lua requested a joypad change, we overwrite the data with lua's changed value for this cycle
			if (overwrite_controller_data[index])
			{
				*input = new_controller_data[index];
				last_controller_data[index] = *input;
				overwrite_controller_data[index] = false;
			}
			vcr_current_sample++;
		} else
		{
			// disconnected controls are forced to have no input during playback
			*input = {0};
		}

	}
}


int vcr_start_record(std::filesystem::path path, uint16_t start_flag)
{
	vcr_stop_all();
	VCR_setReadOnly(FALSE);
	vcr_recent_movies_add(path.string());
	movie_path = path;

	vcr_movie_header = {0};
	vcr_movie_header.magic = MUP_MAGIC;
	vcr_movie_header.version = MUP_VERSION;
	vcr_movie_header.uid = (unsigned long)time(NULL);
	vcr_movie_header.startFlags = start_flag;

	vcr_current_sample = 0;
	m_currentVI = 0;
	movie_inputs = {};

	if (start_flag & MOVIE_START_FROM_SNAPSHOT)
	{
		m_task = e_task::start_recording_from_snapshot;
		savestates_do(strip_extension(path.string()) + ".st", e_st_job::save);
	} else if (start_flag & MOVIE_START_FROM_EXISTING_SNAPSHOT)
	{
		m_task = e_task::start_recording_from_existing_snapshot;
		savestates_do(strip_extension(path.string()) + ".st", e_st_job::load);

		// set this to the normal snapshot flag to maintain compatibility
		vcr_movie_header.startFlags = MOVIE_START_FROM_SNAPSHOT;
	} else
	{
		m_task = e_task::start_recording;
	}

	apply_rom_info(&vcr_movie_header);
	return 0;
}


int vcr_stop_record()
{
	if (m_task != e_task::recording) return -1;

	for (int i = 0; i < movie_inputs.size(); ++i)
	{
		printf("Recorded Input [%d]: %d %d\n", i, movie_inputs[i].Y_AXIS, movie_inputs[i].X_AXIS);
	}

	FILE* f = fopen(movie_path.string().c_str(), "wb");
	fwrite(&vcr_movie_header, sizeof(t_movie_header), 1, f);
	fwrite(movie_inputs.data(), vcr_movie_header.length_samples * sizeof(BUTTONS), 1, f);
	fclose(f);

	m_task = e_task::idle;
	printf("[VCR]: Record stopped. Recorded %ld input samples\n", vcr_movie_header.length_samples);
	EnableEmulationMenuItems(TRUE);
	statusbar_post_text("", 1);
	statusbar_post_text("Stopped recording");

	return 0;
}

bool getSavestatePath(const char* filename, char* outBuffer)
{
	bool found = true;

	char* filenameWithExtension = (char*)malloc(strlen(filename) + 11);
	if (!filenameWithExtension)
		return false;

	strcpy(filenameWithExtension, filename);
	strncat(filenameWithExtension, ".st", 4);
	std::filesystem::path stPath = filenameWithExtension;

	if (std::filesystem::exists(stPath))
		strcpy(outBuffer, filenameWithExtension);
	else
	{
		/* try .savestate (old extension created bc of discord
		trying to display a preview of .st data when uploaded) */
		strcpy(filenameWithExtension, filename);
		strncat(filenameWithExtension, ".savestate", 11);
		stPath = filenameWithExtension;

		if (std::filesystem::exists(stPath))
			strcpy(outBuffer, filenameWithExtension);
		else
			found = false;
	}

	free(filenameWithExtension);
	return found;
}


int vcr_start_playback(std::filesystem::path path, const bool restarting)
{
	is_restarting_flag = false;
	movie_path = path;
	movie_inputs = {};
	vcr_recent_movies_add(path.string());

	auto buf = read_file_buffer(path);

	vcr_movie_header = {0};
	vcr_current_sample = 0;
	m_currentVI = 0;
	memcpy(&vcr_movie_header, buf.data(), sizeof(t_movie_header));

	movie_inputs.resize(vcr_movie_header.length_samples);
	memcpy(movie_inputs.data(), buf.data() + 1024, vcr_movie_header.length_samples * sizeof(BUTTONS));

	if (vcr_movie_header.startFlags & MOVIE_START_FROM_SNAPSHOT)
	{
		// load state
		printf("[VCR]: Loading state...\n");
		savestates_do(strip_extension(movie_path.string()) + ".st", e_st_job::load);
		m_task = e_task::start_playback_from_snapshot;
	} else
	{
		m_task = e_task::start_playback;
	}

	main_dispatcher_invoke(AtPlayMovieLuaCallback);
	return 0;
}

int restartPlayback()
{
	is_restarting_flag = true;
	return vcr_start_playback(VCR_Lastpath, true);
}


int vcr_stop_playback(bool bypass_loop_setting)
{
	if (!bypass_loop_setting && VCR_isLooping())
	{
		return restartPlayback();
	}
	reset_titlebar();

	if (m_task == e_task::start_playback)
	{
		m_task = e_task::idle;
		return 0;
	}

	if (m_task == e_task::playback)
	{
		m_task = e_task::idle;
		printf("[VCR]: Playback stopped (%ld samples played)\n",
		       vcr_current_sample);

		EnableEmulationMenuItems(TRUE);
		statusbar_post_text("", 1);
		statusbar_post_text("Stopped playback");

		main_dispatcher_invoke(AtStopMovieLuaCallback);
		return 0;
	}

	return -1;
}

void VCR_invalidatedCaptureFrame()
{
	captureFrameValid = FALSE;
}

void VCR_updateScreen()
{
	if (!VCR_isCapturing())
	{
		if (!is_frame_skipped()) {
			updateScreen();
		}

		// we always want to invoke atvi, regardless of ff optimization
		if (!hwnd_lua_map.empty())
		{
			main_dispatcher_invoke(AtVILuaCallback);
		}
		return;
	}

	// capturing, update screen and call readscreen, call avi/ffmpeg stuff
	updateScreen();
	if (!hwnd_lua_map.empty())
	{
		main_dispatcher_invoke(AtVILuaCallback);
	}
	void* image = nullptr;
	long width = 0, height = 0;

	auto start = std::chrono::high_resolution_clock::now();
	readScreen(&image, &width, &height);
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> time = (end - start);
	printf("ReadScreen (ffmpeg): %lf ms\n", time.count());

	if (image == nullptr)
	{
		printf("[VCR]: Couldn't read screen (out of memory?)\n");
		return;
	}

	if (captureWithFFmpeg)
	{
		captureManager->WriteVideoFrame((unsigned char*)image,
		                                width * height * 3);

		//free only with external capture, since plugin can't reuse same buffer...
		if (readScreen != vcrcomp_internal_read_screen)
			DllCrtFree(image);

		return;
	}

	if (VCRComp_GetSize() > MAX_AVI_SIZE)
	{
		static char* endptr;
		VCRComp_finishFile(1);
		if (!AVIIncrement)
			endptr = AVIFileName + strlen(AVIFileName) - 4;
		//AVIIncrement
		sprintf(endptr, "%d.avi", ++AVIIncrement);
		VCRComp_startFile(AVIFileName, width, height, get_vis_per_second(ROM_HEADER.Country_code), 0);
	}

	if (Config.synchronization_mode == VCR_SYNC_AUDIO_DUPL || Config.
		synchronization_mode == VCR_SYNC_NONE)
	{
		// AUDIO SYNC
		// This type of syncing assumes the audio is authoratative, and drops or duplicates frames to keep the video as close to
		// it as possible. Some games stop updating the screen entirely at certain points, such as loading zones, which will cause
		// audio to drift away by default. This method of syncing prevents this, at the cost of the video feed possibly freezing or jumping
		// (though in practice this rarely happens - usually a loading scene just appears shorter or something).

		int audio_frames = (int)(m_audioFrame - m_videoFrame + 0.1);
		// i've seen a few games only do ~0.98 frames of audio for a frame, let's account for that here

		if (Config.synchronization_mode == VCR_SYNC_AUDIO_DUPL)
		{
			if (audio_frames < 0)
			{
				printError("Audio frames became negative!");
				vcr_stop_capture();
				goto cleanup;
			}

			if (audio_frames == 0)
			{
				printf("\nDropped Frame! a/v: %Lg/%Lg", m_videoFrame,
				       m_audioFrame);
			} else if (audio_frames > 0)
			{
				if (!VCRComp_addVideoFrame((unsigned char*)image))
				{
					printError(
						"Video codec failure!\nA call to addVideoFrame() (AVIStreamWrite) failed.\nPerhaps you ran out of memory?");
					vcr_stop_capture();
					goto cleanup;
				} else
				{
					m_videoFrame += 1.0;
					audio_frames--;
				}
			}

			// can this actually happen?
			while (audio_frames > 0)
			{
				if (!VCRComp_addVideoFrame((unsigned char*)image))
				{
					printError(
						"Video codec failure!\nA call to addVideoFrame() (AVIStreamWrite) failed.\nPerhaps you ran out of memory?");
					vcr_stop_capture();
					goto cleanup;
				} else
				{
					printf("\nDuped Frame! a/v: %Lg/%Lg", m_videoFrame,
					       m_audioFrame);
					m_videoFrame += 1.0;
					audio_frames--;
				}
			}
		} else /*if (Config.synchronization_mode == VCR_SYNC_NONE)*/
		{
			if (!VCRComp_addVideoFrame((unsigned char*)image))
			{
				printError(
					"Video codec failure!\nA call to addVideoFrame() (AVIStreamWrite) failed.\nPerhaps you ran out of memory?");
				vcr_stop_capture();
				goto cleanup;
			} else
			{
				m_videoFrame += 1.0;
			}
		}
	}

cleanup:
	if (readScreen != vcrcomp_internal_read_screen)
	{
		if (image)
			DllCrtFree(image);
	}
}


void
VCR_aiDacrateChanged(system_type type)
{
	if (VCR_isCapturing())
	{
		printf("Fatal error, audio frequency changed during capture\n");
		vcr_stop_capture();
		return;
	}
	aiDacrateChanged(type);

	m_audioBitrate = (int)ai_register.ai_bitrate + 1;
	switch (type)
	{
	case ntsc:
		m_audioFreq = (int)(48681812 / (ai_register.ai_dacrate + 1));
		break;
	case pal:
		m_audioFreq = (int)(49656530 / (ai_register.ai_dacrate + 1));
		break;
	case mpal:
		m_audioFreq = (int)(48628316 / (ai_register.ai_dacrate + 1));
		break;
	default:
		assert(false);
		break;
	}
}

// assumes: len <= writeSize
static void writeSound(char* buf, int len, int minWriteSize, int maxWriteSize,
                       BOOL force)
{
	if ((len <= 0 && !force) || len > maxWriteSize)
		return;

	if (soundBufPos + len > minWriteSize || force)
	{
		int len2 = VCR_getResampleLen(44100, m_audioFreq, m_audioBitrate,
		                              soundBufPos);
		if ((len2 % 8) == 0 || len > maxWriteSize)
		{
			static short* buf2 = NULL;
			len2 = VCR_resample(&buf2, 44100,
			                    reinterpret_cast<short*>(soundBuf), m_audioFreq,
			                    m_audioBitrate, soundBufPos);
			if (len2 > 0)
			{
				if ((len2 % 4) != 0)
				{
					printf(
						"[VCR]: Warning: Possible stereo sound error detected.\n");
					fprintf(
						stderr,
						"[VCR]: Warning: Possible stereo sound error detected.\n");
				}
				if (!VCRComp_addAudioData((unsigned char*)buf2, len2))
				{
					printError(
						"Audio output failure!\nA call to addAudioData() (AVIStreamWrite) failed.\nPerhaps you ran out of memory?");
					vcr_stop_capture();
				}
			}
			soundBufPos = 0;
		}
	}

	if (len > 0)
	{
		if ((unsigned int)(soundBufPos + len) > SOUND_BUF_SIZE * sizeof(char))
		{
			MessageBox(0, "Fatal error", "Sound buffer overflow", MB_ICONERROR);
			printf("SOUND BUFFER OVERFLOW\n");
			return;
		}
#ifdef _DEBUG
		else
		{
			long double pro = (long double)(soundBufPos + len) * 100 / (
				SOUND_BUF_SIZE * sizeof(char));
			if (pro > 75) printf("---!!!---");
			printf("sound buffer: %.2f%%\n", pro);
		}
#endif
		memcpy(soundBuf + soundBufPos, (char*)buf, len);
		soundBufPos += len;
		m_audioFrame += ((len / 4) / (long double)m_audioFreq) *
			get_vis_per_second(ROM_HEADER.Country_code);
	}
}

// calculates how long the audio data will last
float GetPercentOfFrame(int aiLen, int audioFreq, int audioBitrate)
{
	int limit = get_vis_per_second(ROM_HEADER.Country_code);
	float viLen = 1.f / (float)limit; //how much seconds one VI lasts
	float time = (float)(aiLen * 8) / ((float)audioFreq * 2.f * (float)
		audioBitrate); //how long the buffer can play for
	return time / viLen; //ratio
}

void VCR_aiLenChanged()
{
	short* p = reinterpret_cast<short*>((char*)rdram + (ai_register.ai_dram_addr
		& 0xFFFFFF));
	char* buf = (char*)p;
	int aiLen = (int)ai_register.ai_len;
	aiLenChanged();

	// hack - mupen64 updates bitrate after calling aiDacrateChanged
	m_audioBitrate = (int)ai_register.ai_bitrate + 1;

	if (m_capture == 0)
		return;

	if (captureWithFFmpeg)
	{
		captureManager->WriteAudioFrame(buf, aiLen);
		return;
	}

	if (aiLen > 0)
	{
		int len = aiLen;
		int writeSize = 2 * m_audioFreq;
		// we want (writeSize * 44100 / m_audioFreq) to be an integer

		/*
				// TEMP PARANOIA
				if(len > writeSize)
				{
					char str [256];
					printf("Sound AVI Output Failure: %d > %d", len, writeSize);
					fprintf(stderr, "Sound AVI Output Failure: %d > %d", len, writeSize);
					sprintf(str, "Sound AVI Output Failure: %d > %d", len, writeSize);
					printError(str);
					printWarning(str);
					exit(0);
				}
		*/

		if (Config.synchronization_mode == VCR_SYNC_VIDEO_SNDROP || Config.
			synchronization_mode == VCR_SYNC_NONE)
		{
			// VIDEO SYNC
			// This is the original syncing code, which adds silence to the audio track to get it to line up with video.
			// The N64 appears to have the ability to arbitrarily disable its sound processing facilities and no audio samples
			// are generated. When this happens, the video track will drift away from the audio. This can happen at load boundaries
			// in some games, for example.
			//
			// The only new difference here is that the desync flag is checked for being greater than 1.0 instead of 0.
			// This is because the audio and video in mupen tend to always be diverged just a little bit, but stay in sync
			// over time. Checking if desync is not 0 causes the audio stream to to get thrashed which results in clicks
			// and pops.

			long double desync = m_videoFrame - m_audioFrame;

			if (Config.synchronization_mode == VCR_SYNC_NONE) // HACK
				desync = 0;

			if (desync > 1.0)
			{
				int len3;
				printf(
					"[VCR]: Correcting for A/V desynchronization of %+Lf frames\n",
					desync);
				len3 = (int)(m_audioFreq / (long double)get_vis_per_second(ROM_HEADER.Country_code)) * (
					int)desync;
				len3 <<= 2;

				int emptySize = len3 > writeSize ? writeSize : len3;
				int i;

				for (i = 0; i < emptySize; i += 4)
					*reinterpret_cast<long*>(soundBufEmpty + i) = lastSound;

				while (len3 > writeSize)
				{
					writeSound(soundBufEmpty, writeSize, m_audioFreq, writeSize,
					           FALSE);
					len3 -= writeSize;
				}
				writeSound(soundBufEmpty, len3, m_audioFreq, writeSize, FALSE);
			} else if (desync <= -10.0)
			{
				printf(
					"[VCR]: Waiting from A/V desynchronization of %+Lf frames\n",
					desync);
			}
		}

		writeSound(buf, len, m_audioFreq, writeSize, FALSE);

		lastSound = *(reinterpret_cast<long*>(buf + len) - 1);
	}
}

void UpdateTitleBarCapture(const char* filename)
{
	// TODO: reimplement but less ugly
}

bool vcr_start_capture(const char* path, bool show_codec_dialog)
{
	extern BOOL emu_paused;
	BOOL was_paused = emu_paused;
	if (!emu_paused)
	{
		pauseEmu(TRUE);
	}

	if (readScreen == nullptr)
	{
		printf("readScreen not implemented by graphics plugin. Falling back...\n");
		readScreen = vcrcomp_internal_read_screen;
	}

	memset(soundBufEmpty, 0, std::size(soundBufEmpty));
	memset(soundBuf, 0, std::size(soundBuf));
	lastSound = 0;
	m_videoFrame = 0.0;
	m_audioFrame = 0.0;

	// fill in window size at avi start, which can't change
	// scrap whatever was written there even if window didnt change, for safety
	vcrcomp_window_info = {0};
	get_window_info(mainHWND, vcrcomp_window_info);
	long width = vcrcomp_window_info.width & ~3;
	long height = vcrcomp_window_info.height & ~3;

	VCRComp_startFile(path, width, height, get_vis_per_second(ROM_HEADER.Country_code), show_codec_dialog);
	m_capture = 1;
	captureWithFFmpeg = false;
	strncpy(AVIFileName, path, PATH_MAX);
	Config.avi_capture_path = path;


	// toolbar could get captured in AVI, so we disable it
	toolbar_set_visibility(0);
	UpdateTitleBarCapture(AVIFileName);
	EnableEmulationMenuItems(TRUE);
	SetWindowLong(mainHWND, GWL_STYLE, GetWindowLong(mainHWND, GWL_STYLE) & ~WS_MINIMIZEBOX);
	// we apply WS_EX_LAYERED to fix off-screen blitting (off-screen window portions are not included otherwise)
	SetWindowLong(mainHWND, GWL_EXSTYLE, GetWindowLong(mainHWND, GWL_EXSTYLE) | WS_EX_LAYERED);

	VCR_invalidatedCaptureFrame();

	if (!was_paused || (m_task == e_task::playback || m_task == e_task::start_playback || m_task
		== e_task::start_playback_from_snapshot))
	{
		resumeEmu(TRUE);
	}

	printf("[VCR]: Starting capture...\n");

	return true;
}

/// <summary>
/// Start capture using ffmpeg, if this function fails, manager (process and pipes) isn't created and error is returned.
/// </summary>
/// <param name="outputName">name of video file output</param>
/// <param name="arguments">additional ffmpeg params (output stream only)</param>
/// <returns></returns>
int VCR_StartFFmpegCapture(const std::string& outputName,
                           const std::string& arguments)
{
	if (!emu_launched) return INIT_EMU_NOT_LAUNCHED;
	if (captureManager != nullptr)
	{
#ifdef _DEBUG
		printf(
			"[VCR] Attempted to start ffmpeg capture when it was already started\n");
#endif
		return INIT_ALREADY_RUNNING;
	}
	t_window_info sInfo{};
	get_window_info(mainHWND, sInfo);

	InitReadScreenFFmpeg(sInfo);
	captureManager = std::make_unique<FFmpegManager>(
		sInfo.width, sInfo.height, get_vis_per_second(ROM_HEADER.Country_code), m_audioFreq,
		arguments + " " + outputName);

	auto err = captureManager->initError;
	if (err != INIT_SUCCESS)
		captureManager.reset();
	else
	{
		UpdateTitleBarCapture(outputName.data());
		m_capture = 1;
		captureWithFFmpeg = 1;
	}
#ifdef _DEBUG
	if (err == INIT_SUCCESS)
		printf("[VCR] ffmpeg capture started\n");
	else
		printf("[VCR] Could not start ffmpeg capture\n");
#endif
	return err;
}

void VCR_StopFFmpegCapture()
{
	if (captureManager == nullptr) return; // no error code but its no big deal
	m_capture = 0;
	//this must be first in case object is being destroyed and other thread still sees m_capture=1
	captureManager.reset();
	//apparently closing the pipes is enough to tell ffmpeg the movie ended.
#ifdef _DEBUG
	printf("[VCR] ffmpeg capture stopped\n");
#endif
}

void
VCR_toggleReadOnly()
{
	VCR_setReadOnly(!m_readOnly);

	statusbar_post_text(m_readOnly ? "Read" : "Read-write");
}

void
VCR_toggleLoopMovie()
{
	Config.is_movie_loop_enabled ^= 1;

	extern HWND mainHWND;
	CheckMenuItem(GetMenu(mainHWND), ID_LOOP_MOVIE,
	              MF_BYCOMMAND | (Config.is_movie_loop_enabled
		                              ? MFS_CHECKED
		                              : MFS_UNCHECKED));

	if (emu_launched)
		statusbar_post_text(Config.is_movie_loop_enabled
								 ? "Movies restart after ending"
								 : "Movies stop after ending");
}


int vcr_stop_capture()
{
	if (captureWithFFmpeg)
	{
		VCR_StopFFmpegCapture();
		return 0;
	}

	m_capture = 0;
	writeSound(NULL, 0, m_audioFreq, m_audioFreq * 2, TRUE);
	VCR_invalidatedCaptureFrame();

	// re-enable the toolbar (m_capture==0 causes this call to do that)
	// check previous update_toolbar_visibility call
	toolbar_set_visibility(Config.is_toolbar_enabled);

	SetWindowPos(mainHWND, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	reset_titlebar();
	SetWindowLong(mainHWND, GWL_STYLE, GetWindowLong(mainHWND, GWL_STYLE) | WS_MINIMIZEBOX);
	// we remove WS_EX_LAYERED again, because dwm sucks at dealing with layered top-level windows
	SetWindowLong(mainHWND, GWL_EXSTYLE, GetWindowLong(mainHWND, GWL_EXSTYLE) & ~WS_EX_LAYERED);

	VCRComp_finishFile(0);
	AVIIncrement = 0;
	printf("[VCR]: Capture finished.\n");
	return 0;
}


void vcr_stop_all()
{
	if (continue_vcr_on_restart_mode)
		return;

	switch (m_task)
	{
	case e_task::start_recording:
	case e_task::start_recording_from_snapshot:
	case e_task::recording:
		vcr_stop_record();
		break;
	case e_task::start_playback:
	case e_task::start_playback_from_snapshot:
	case e_task::playback:
		vcr_stop_playback(false);
		break;
	default:
		break;
	}

	if (m_capture)
		vcr_stop_capture();
}

void vcr_update_statusbar()
{
	BUTTONS b = last_controller_data[0];
	std::string input_string = std::format("({}, {}) ", (int)b.Y_AXIS, (int)b.X_AXIS);
	if (b.START_BUTTON) input_string += "S";
	if (b.Z_TRIG) input_string += "Z";
	if (b.A_BUTTON) input_string += "A";
	if (b.B_BUTTON) input_string += "B";
	if (b.L_TRIG) input_string += "L";
	if (b.R_TRIG) input_string += "R";
	if (b.U_CBUTTON || b.D_CBUTTON || b.L_CBUTTON ||
		b.R_CBUTTON)
	{
		input_string += " C";
		if (b.U_CBUTTON) input_string += "^";
		if (b.D_CBUTTON) input_string += "v";
		if (b.L_CBUTTON) input_string += "<";
		if (b.R_CBUTTON) input_string += ">";
	}
	if (b.U_DPAD || b.D_DPAD || b.L_DPAD || b.
		R_DPAD)
	{
		input_string += " D";
		if (b.U_DPAD) input_string += "^";
		if (b.D_DPAD) input_string += "v";
		if (b.L_DPAD) input_string += "<";
		if (b.R_DPAD) input_string += ">";
	}

	if (VCR_isRecording())
	{
		std::string text = std::format("{} ({}) ", m_currentVI, vcr_current_sample);
		statusbar_post_text(text + input_string);
		statusbar_post_text(std::format("{} rr", vcr_movie_header.rerecord_count), 1);
	}

	if (VCR_isPlaying())
	{
		std::string text = std::format("{} / {} ({} / {}) ", m_currentVI, VCR_getLengthVIs(), vcr_current_sample, VCR_getLengthSamples());
		statusbar_post_text(text + input_string);
	}

	if (!VCR_isActive())
	{
		statusbar_post_text(input_string);
	}
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////// Recent Movies //////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void vcr_recent_movies_build(int32_t reset)
{
	HMENU h_menu = GetMenu(mainHWND);

	for (size_t i = 0; i < Config.recent_movie_paths.size(); i++)
	{
		if (Config.recent_movie_paths[i].empty())
		{
			continue;
		}
		DeleteMenu(h_menu, ID_RECENTMOVIES_FIRST + i, MF_BYCOMMAND);
	}

	if (reset)
	{
		Config.recent_movie_paths.clear();
	}

	HMENU h_sub_menu = GetSubMenu(h_menu, 3);
	h_sub_menu = GetSubMenu(h_sub_menu, 6);

	MENUITEMINFO menu_info = {0};
	menu_info.cbSize = sizeof(MENUITEMINFO);
	menu_info.fMask = MIIM_TYPE | MIIM_ID;
	menu_info.fType = MFT_STRING;
	menu_info.fState = MFS_ENABLED;

	for (size_t i = 0; i < Config.recent_movie_paths.size(); i++)
	{
		if (Config.recent_movie_paths[i].empty())
		{
			continue;
		}
		menu_info.dwTypeData = const_cast<LPSTR>(Config.recent_movie_paths[i].c_str());
		menu_info.cch = strlen(menu_info.dwTypeData);
		menu_info.wID = ID_RECENTMOVIES_FIRST + i;
		InsertMenuItem(h_sub_menu, i + 3, TRUE, &menu_info);
	}
}

void vcr_recent_movies_add(const std::string path)
{
	if (Config.is_recent_movie_paths_frozen)
	{
		return;
	}
	if (Config.recent_movie_paths.size() > 5)
	{
		Config.recent_movie_paths.pop_back();
	}
	std::erase(Config.recent_movie_paths, path);
	Config.recent_movie_paths.insert(Config.recent_movie_paths.begin(), path);
	vcr_recent_movies_build();
}

int32_t vcr_recent_movies_play(uint16_t menu_item_id)
{
	int index = menu_item_id - ID_RECENTMOVIES_FIRST;
	if (index >= 0 && index < Config.recent_movie_paths.size())
	{
		VCR_setReadOnly(TRUE);
		return vcr_start_playback(Config.recent_movie_paths[index], false);
	}
	return 0;
}
