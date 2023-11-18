//#include "../config.h"
#include <cassert>
#include <cstring>
#include <malloc.h>
#include <memory>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdio>
#include <ctime>
#include <chrono>
#include "win/main_win.h"
#include "win/features/Statusbar.hpp"
#include "win/features/Toolbar.hpp"

#include "../lua/LuaConsole.h"
#include "vcr.h"
#include "vcr_compress.h"
#include "vcr_resample.h"

//ffmpeg
#include "ffmpeg_capture/ffmpeg_capture.hpp"

#include "plugin.hpp"
#include "rom.h"
#include "savestates.h"
#include "../memory/memory.h"

#ifdef _MSC_VER
#define SNPRINTF	_snprintf
#define STRCASECMP	_stricmp
#define STRNCASECMP	_strnicmp
#else
#include <unistd.h>
#endif
//#include <zlib.h>

#include <commctrl.h> // for SendMessage, SB_SETTEXT
#include <Windows.h> // for truncate functions
#include <../../winproject/resource.h> // for EMU_RESET
#include "win/Config.hpp" //config struct
#include <WinUser.h>
#include "win/Commandline.h"

#ifdef _DEBUG
#endif

#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif

// M64\0x1a
enum
{
	mup_magic = (0x1a34364d),
	mup_version = (3),
	mup_header_size_old = (512)
};

#define MUP_HEADER_SIZE (sizeof(t_movie_header))
#define MUP_HEADER_SIZE_CUR (vcr_movie_header.version <= 2 ? MUP_HEADER_SIZE_OLD : MUP_HEADER_SIZE)
enum { max_avi_size = 0x7B9ACA00 };

BOOL dont_play = false;

enum { buffer_growth_size = (4096) };

[[maybe_unused]] static const char* m_err_code_name[] =
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

static char avi_file_name[PATH_MAX];
std::filesystem::path movie_path;
std::vector<BUTTONS> movie_inputs;

t_movie_header vcr_movie_header = {0};
static BOOL m_read_only = FALSE;

int64_t vcr_current_sample = -1;
// should = length_samples when recording, and be < length_samples when playing
int m_current_vi = -1;

static int m_capture = 0; // capture movie
static int m_audio_freq = 33000; //0x30018;
static int m_audio_bitrate = 16; // 16 bits
static long double m_video_frame = 0;
static long double m_audio_frame = 0;
#define SOUND_BUF_SIZE (44100*2*2) // 44100=1s sample, soundbuffer capable of holding 4s future data in circular buffer

static char sound_buf[SOUND_BUF_SIZE];
static char sound_buf_empty[SOUND_BUF_SIZE];
static int sound_buf_pos = 0;
long last_sound = 0;
int avi_increment = 0;
int title_length;
char vcr_lastpath[MAX_PATH];
bool is_restarting_flag = false;

bool capture_with_ffmpeg = true;
std::unique_ptr<FFmpegManager> capture_manager;
uint64_t screen_updates = 0;

static int restart_playback();

char* strtrimext(const char* my_str)
{
	char* ret_str;
	if (my_str == nullptr) return nullptr;
	if ((ret_str = (char*)malloc(strlen(my_str) + 1)) == nullptr) return nullptr;
	strcpy(ret_str, my_str);
	if (char* last_ext = strrchr(ret_str, '.'); last_ext != nullptr)
		*last_ext = '\0';
	return ret_str;
}

void print_error(const char* str)
{
	MessageBox(nullptr, str, "Error", MB_OK | MB_ICONERROR);
}


static void hard_reset_and_clear_all_save_data(const bool clear)
{
	extern BOOL clear_sram_on_restart_mode;
	clear_sram_on_restart_mode = clear;
	continue_vcr_on_restart_mode = TRUE;
	if (clear)
		printf("Clearing save data...\n");
	else
		printf("Playing movie without clearing save data\n");
	SendMessage(main_hwnd, WM_COMMAND, EMU_RESET, 0);
}

void apply_rom_info(t_movie_header* header)
{
	header->vis_per_second = static_cast<uint8_t>(get_vis_per_second(ROM_HEADER.Country_code));
	header->controller_flags = 0;
	header->num_controllers = 0;

	for (int i = 0; i < 4; ++i)
	{
		if (controls[i].Plugin == controller_extension::mempak)
		{
			header->controller_flags |= CONTROLLER_X_MEMPAK(i);
		}
		if (controls[i].Plugin == controller_extension::rumblepak)
		{
			header->controller_flags |= CONTROLLER_X_RUMBLE(i);
		}

		if (!controls[i].Present)
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


int parse_header(std::vector<uint8_t>& buffer, t_movie_header* out_header)
{
	// movie doesnt have a complete header
	if (buffer.size() < sizeof(t_movie_header))
	{
		return false;
	}

	uint8_t* ptr = buffer.data();
	t_movie_header header = {0};

	memread(&ptr, &header, sizeof(t_movie_header));

	if (header.magic != mup_magic)
		return wrong_format;

	if (header.version <= 0 || header.version > mup_version)
		return wrong_version;

	if (header.version == 1 || header.version == 2)
	{
		// attempt to recover screwed-up plugin data caused by
		// version mishandling and format problems of first versions

#define IS_ALPHA(x) (((x) >= 'A' && (x) <= 'Z') || ((x) >= 'a' && (x) <= 'z') || ((x) == '1'))
		int i;
		for (i = 0; i < 56 + 64; i++)
			if (IS_ALPHA(header.reserved_bytes[i])
				&& IS_ALPHA(header.reserved_bytes[i + 64])
				&& IS_ALPHA(header.reserved_bytes[i + 64 + 64])
				&& IS_ALPHA(header.reserved_bytes[i + 64 + 64 + 64]))
				break;
		if (i != 56 + 64)
		{
			memmove(header.video_plugin_name, header.reserved_bytes + i,
			        256);
		} else
		{
			for (i = 0; i < 56 + 64; i++)
				if (IS_ALPHA(header.reserved_bytes[i])
					&& IS_ALPHA(header.reserved_bytes[i + 64])
					&& IS_ALPHA(header.reserved_bytes[i + 64 + 64]))
					break;
			if (i != 56 + 64)
				memmove(header.audio_plugin_name, header.reserved_bytes + i,
				        256 - 64);
			else
			{
				for (i = 0; i < 56 + 64; i++)
					if (IS_ALPHA(header.reserved_bytes[i])
						&& IS_ALPHA(header.reserved_bytes[i + 64]))
						break;
				if (i != 56 + 64)
					memmove(header.input_plugin_name,
					        header.reserved_bytes + i, 256 - 64 - 64);
				else
				{
					for (i = 0; i < 56 + 64; i++)
						if (IS_ALPHA(header.reserved_bytes[i]))
							break;
					if (i != 56 + 64)
						memmove(header.rsp_plugin_name,
						        header.reserved_bytes + i,
						        256 - 64 - 64 - 64);
					else
						strncpy(header.rsp_plugin_name, "(unknown)", 64);

					strncpy(header.input_plugin_name, "(unknown)", 64);
				}
				strncpy(header.audio_plugin_name, "(unknown)", 64);
			}
			strncpy(header.video_plugin_name, "(unknown)", 64);
		}
		// attempt to convert old author and description to utf8
		strncpy(header.author, header.old_author_info, 48);
		strncpy(header.description, header.old_description, 80);
		header.version = mup_version;
	}

	*out_header = header;
	return success;
}

bool vcr_parse_header(std::vector<uint8_t>& buffer, t_movie_header* header)
{
	return parse_header(buffer, header) == success;
}

bool vcr_restore(std::vector<BUTTONS>& input_buffer)
{
	if (vcr_is_idle())
	{
		return true;
	}

	vcr_current_sample = input_buffer.size();

	if (vcr_get_read_only())
	{
		// In RO mode, we only want to rewind the input buffer pointer
		m_task = e_task::playback;
		return true;
	} else
	{
		// In RW mode, we want to turn movie playback into a recording and overwrite the input buffer
		// TODO: Also update titlebar?
		movie_inputs.resize(input_buffer.size());
		memcpy(movie_inputs.data(), input_buffer.data(), std::size(input_buffer));

		m_task = e_task::recording;
		vcr_movie_header.length_samples = movie_inputs.size();
		enable_emulation_menu_items(TRUE);
	}

	return false;
}

void vcr_clear_save_data()
{
	{
		if (FILE* f = fopen(get_sram_path().string().c_str(), "wb"))
		{
			extern unsigned char sram[0x8000];
			for (unsigned char& i : sram) i = 0;
			fwrite(sram, 1, 0x8000, f);
			fclose(f);
		}
	}
	{
		if (FILE* f = fopen(get_eeprom_path().string().c_str(), "wb"))
		{
			extern unsigned char eeprom[0x8000];
			for (int i = 0; i < 0x800; i++) eeprom[i] = 0;
			fwrite(eeprom, 1, 0x800, f);
			fclose(f);
		}
	}
	{
		if (FILE* f = fopen(get_mempak_path().string().c_str(), "wb"))
		{
			extern unsigned char mempack[4][0x8000];
			for (auto& j : mempack)
			{
				for (int i = 0; i < 0x800; i++) j[i] = 0;
				fwrite(j, 1, 0x800, f);
			}
			fclose(f);
		}
	}
}


BOOL
vcr_is_active()
{
	return (m_task == e_task::recording || m_task == e_task::playback) ? TRUE : FALSE;
}

BOOL
vcr_is_idle()
{
	return (m_task == e_task::idle) ? TRUE : FALSE;
}

BOOL
vcr_is_starting()
{
	return (m_task == e_task::start_playback || m_task == e_task::start_playback_from_snapshot)
		       ? TRUE
		       : FALSE;
}

BOOL
vcr_is_starting_and_just_restarted()
{
	if (extern BOOL just_restarted_flag; m_task == e_task::start_playback &&
		!continue_vcr_on_restart_mode && just_restarted_flag)
	{
		just_restarted_flag = FALSE;
		vcr_current_sample = 0;
		m_current_vi = 0;
		m_task = e_task::playback;
		return TRUE;
	}

	return FALSE;
}

BOOL
vcr_is_playing()
{
	return (m_task == e_task::playback) ? TRUE : FALSE;
}

BOOL
vcr_is_recording()
{
	return (m_task == e_task::recording) ? TRUE : FALSE;
}

BOOL
vcr_is_capturing()
{
	return m_capture ? TRUE : FALSE;
}

BOOL
vcr_get_read_only()
{
	return m_read_only;
}

void
vcr_set_read_only(const BOOL val)
{
	extern HWND main_hwnd;
	if (m_read_only != val)
		CheckMenuItem(GetMenu(main_hwnd), EMU_VCRTOGGLEREADONLY,
		              MF_BYCOMMAND | (val ? MFS_CHECKED : MFS_UNCHECKED));
	m_read_only = val;
}

bool vcr_is_looping()
{
	return Config.is_movie_loop_enabled;
}

bool vcr_is_restarting()
{
	return is_restarting_flag;
}

void vcr_set_loop_movie(const bool val)
{
	extern HWND main_hwnd;
	if (vcr_is_looping() != val)
		CheckMenuItem(GetMenu(main_hwnd), ID_LOOP_MOVIE,
		              MF_BYCOMMAND | (val ? MFS_CHECKED : MFS_UNCHECKED));
	Config.is_movie_loop_enabled = val;
}

unsigned long vcr_get_length_v_is()
{
	return vcr_is_active() ? vcr_movie_header.length_vis : 0;
}

unsigned long vcr_get_length_samples()
{
	return vcr_is_active() ? vcr_movie_header.length_samples : 0;
}

void vcr_set_length_v_is(const unsigned long val)
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
				m_current_vi = 0;
				m_task = e_task::recording;
				*input = {0};
				EnableMenuItem(GetMenu(main_hwnd), ID_STOP_RECORD, MF_ENABLED);
			} else
			{
				printf("[VCR]: Starting recording...\n");
				hard_reset_and_clear_all_save_data(
					!(vcr_movie_header.start_flags & movie_start_from_eeprom));
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
				m_current_vi = 0;
				m_task = e_task::playback;
			} else
			{
				printf("[VCR]: Starting playback...\n");
				hard_reset_and_clear_all_save_data(
					!(vcr_movie_header.start_flags & movie_start_from_eeprom));
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

		printf("Recording Input [%lld] (len %d): %d %d\n", vcr_current_sample, movie_inputs.size(), input->Y_AXIS, input->X_AXIS);
		movie_inputs.push_back(*input);
		vcr_movie_header.length_samples++;
		vcr_current_sample++;

		if (scheduled_restart)
		{
			reset_emu();
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
			*input = movie_inputs[(int)vcr_current_sample];
			setKeys(index, movie_inputs[(int)vcr_current_sample]);

			//no readable code because 120 star tas can't get this right >:(
			if (input->Value == 0xC000)
			{
				continue_vcr_on_restart_mode = true;
				reset_emu();
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


int vcr_start_record(const std::filesystem::path& path, const uint16_t start_flag)
{
	vcr_stop_all();
	vcr_set_read_only(FALSE);
	vcr_recent_movies_add(path.string());
	movie_path = path;

	vcr_movie_header = {0};
	vcr_movie_header.magic = mup_magic;
	vcr_movie_header.version = mup_version;
	vcr_movie_header.uid = (unsigned long)time(nullptr);
	vcr_movie_header.start_flags = start_flag;

	vcr_current_sample = 0;
	m_current_vi = 0;
	movie_inputs = {};

	if (start_flag & movie_start_from_snapshot)
	{
		m_task = e_task::start_recording_from_snapshot;
		savestates_do(strip_extension(path.string()) + ".st", e_st_job::save);
	} else if (start_flag & movie_start_from_existing_snapshot)
	{
		m_task = e_task::start_recording_from_existing_snapshot;
		savestates_do(strip_extension(path.string()) + ".st", e_st_job::load);

		// set this to the normal snapshot flag to maintain compatibility
		vcr_movie_header.start_flags = movie_start_from_snapshot;
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

	for (int i = 0; (unsigned int)i < movie_inputs.size(); ++i)
	{
		printf("Recorded Input [%d]: %d %d\n", i, movie_inputs[i].Y_AXIS, movie_inputs[i].X_AXIS);
	}

	FILE* f = fopen(movie_path.string().c_str(), "wb");
	fwrite(&vcr_movie_header, sizeof(t_movie_header), 1, f);
	fwrite(movie_inputs.data(), vcr_movie_header.length_samples * sizeof(BUTTONS), 1, f);
	fclose(f);

	m_task = e_task::idle;
	printf("[VCR]: Record stopped. Recorded %ld input samples\n", vcr_movie_header.length_samples);
	enable_emulation_menu_items(TRUE);
	statusbar_post_text("", 1);
	statusbar_post_text("Stopped recording");

	return 0;
}

bool get_savestate_path(const char* filename, char* out_buffer)
{
	bool found = true;

	char* filenameWithExtension = (char*)malloc(strlen(filename) + 11);
	if (!filenameWithExtension)
		return false;

	strcpy(filenameWithExtension, filename);
	strncat(filenameWithExtension, ".st", 4);

	if (std::filesystem::path st_path = filenameWithExtension; std::filesystem::exists(st_path))
		strcpy(out_buffer, filenameWithExtension);
	else
	{
		/* try .savestate (old extension created bc of discord
		trying to display a preview of .st data when uploaded) */
		strcpy(filenameWithExtension, filename);
		strncat(filenameWithExtension, ".savestate", 11);
		st_path = filenameWithExtension;

		if (std::filesystem::exists(st_path))
			strcpy(out_buffer, filenameWithExtension);
		else
			found = false;
	}

	free(filenameWithExtension);
	return found;
}


int vcr_start_playback(const std::filesystem::path& path, const bool restarting)
{
	is_restarting_flag = false;
	movie_path = path;
	movie_inputs = {};
	vcr_recent_movies_add(path.string());

	const auto buf = read_file_buffer(path);

	vcr_movie_header = {0};
	vcr_current_sample = 0;
	m_current_vi = 0;
	memcpy(&vcr_movie_header, buf.data(), sizeof(t_movie_header));

	movie_inputs.resize(vcr_movie_header.length_samples);
	memcpy(movie_inputs.data(), buf.data() + sizeof(t_movie_header), vcr_movie_header.length_samples * sizeof(BUTTONS));
	strcpy(vcr_lastpath, path.string().c_str());

	if (vcr_movie_header.start_flags & movie_start_from_snapshot)
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

int restart_playback()
{
	is_restarting_flag = true;
	return vcr_start_playback(vcr_lastpath, true);
}


int vcr_stop_playback(const bool bypass_loop_setting)
{
	if (!bypass_loop_setting && vcr_is_looping())
	{
		return restart_playback();
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
		printf("[VCR]: Playback stopped (%lld samples played)\n",
		       vcr_current_sample);

		enable_emulation_menu_items(TRUE);
		statusbar_post_text("", 1);
		statusbar_post_text("Stopped playback");

		main_dispatcher_invoke(AtStopMovieLuaCallback);
		return 0;
	}

	return -1;
}

void vcr_update_screen()
{
	if (!vcr_is_capturing())
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

	const auto start = std::chrono::high_resolution_clock::now();
	readScreen(&image, &width, &height);
	const auto end = std::chrono::high_resolution_clock::now();
	const std::chrono::duration<double, std::milli> time = (end - start);
	printf("ReadScreen (ffmpeg): %lf ms\n", time.count());

	if (image == nullptr)
	{
		printf("[VCR]: Couldn't read screen (out of memory?)\n");
		return;
	}

	if (capture_with_ffmpeg)
	{
		capture_manager->WriteVideoFrame((unsigned char*)image,
		                                width * height * 3);

		//free only with external capture, since plugin can't reuse same buffer...
		if (readScreen != vcrcomp_internal_read_screen)
			DllCrtFree(image);

		return;
	}

	if (vcr_comp_get_size() > max_avi_size)
	{
		static char* endptr;
		vcr_comp_finish_file(1);
		if (!avi_increment)
			endptr = avi_file_name + strlen(avi_file_name) - 4;
		//AVIIncrement
		sprintf(endptr, "%d.avi", ++avi_increment);
		vcr_comp_start_file(avi_file_name, width, height, (int)get_vis_per_second(ROM_HEADER.Country_code), 0);
	}

	if (Config.synchronization_mode == vcr_sync_audio_dupl || Config.
		synchronization_mode == vcr_sync_none)
	{
		// AUDIO SYNC
		// This type of syncing assumes the audio is authoratative, and drops or duplicates frames to keep the video as close to
		// it as possible. Some games stop updating the screen entirely at certain points, such as loading zones, which will cause
		// audio to drift away by default. This method of syncing prevents this, at the cost of the video feed possibly freezing or jumping
		// (though in practice this rarely happens - usually a loading scene just appears shorter or something).

		int audio_frames = (int)(m_audio_frame - m_video_frame + 0.1);
		// i've seen a few games only do ~0.98 frames of audio for a frame, let's account for that here

		if (Config.synchronization_mode == vcr_sync_audio_dupl)
		{
			if (audio_frames < 0)
			{
				print_error("Audio frames became negative!");
				vcr_stop_capture();
				goto cleanup;
			}

			if (audio_frames == 0)
			{
				printf("\nDropped Frame! a/v: %Lg/%Lg", m_video_frame,
				       m_audio_frame);
			} else if (audio_frames > 0)
			{
				if (!vcr_comp_add_video_frame((unsigned char*)image))
				{
					print_error(
						"Video codec failure!\nA call to addVideoFrame() (AVIStreamWrite) failed.\nPerhaps you ran out of memory?");
					vcr_stop_capture();
					goto cleanup;
				} else
				{
					m_video_frame += 1.0;
					audio_frames--;
				}
			}

			// can this actually happen?
			while (audio_frames > 0)
			{
				if (!vcr_comp_add_video_frame((unsigned char*)image))
				{
					print_error(
						"Video codec failure!\nA call to addVideoFrame() (AVIStreamWrite) failed.\nPerhaps you ran out of memory?");
					vcr_stop_capture();
					goto cleanup;
				} else
				{
					printf("\nDuped Frame! a/v: %Lg/%Lg", m_video_frame,
					       m_audio_frame);
					m_video_frame += 1.0;
					audio_frames--;
				}
			}
		} else /*if (Config.synchronization_mode == VCR_SYNC_NONE)*/
		{
			if (!vcr_comp_add_video_frame((unsigned char*)image))
			{
				print_error(
					"Video codec failure!\nA call to addVideoFrame() (AVIStreamWrite) failed.\nPerhaps you ran out of memory?");
				vcr_stop_capture();
				goto cleanup;
			} else
			{
				m_video_frame += 1.0;
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
vcr_ai_dacrate_changed(const system_type type)
{
	if (vcr_is_capturing())
	{
		printf("Fatal error, audio frequency changed during capture\n");
		vcr_stop_capture();
		return;
	}
	aiDacrateChanged(type);

	m_audio_bitrate = (int)ai_register.ai_bitrate + 1;
	switch (type)
	{
	case ntsc:
		m_audio_freq = (int)(48681812 / (ai_register.ai_dacrate + 1));
		break;
	case pal:
		m_audio_freq = (int)(49656530 / (ai_register.ai_dacrate + 1));
		break;
	case mpal:
		m_audio_freq = (int)(48628316 / (ai_register.ai_dacrate + 1));
		break;
	default:
		assert(false);
		break;
	}
}

// assumes: len <= writeSize
static void write_sound(char* buf, const int len, const int min_write_size, const int max_write_size,
                        const BOOL force)
{
	if ((len <= 0 && !force) || len > max_write_size)
		return;

	if (sound_buf_pos + len > min_write_size || force)
	{
		if (int len2 = VCR_getResampleLen(44100, m_audio_freq, m_audio_bitrate,
		                                  sound_buf_pos); (len2 % 8) == 0 || len > max_write_size)
		{
			static short* buf2 = nullptr;
			len2 = VCR_resample(&buf2, 44100,
			                    reinterpret_cast<short*>(sound_buf), m_audio_freq,
			                    m_audio_bitrate, sound_buf_pos);
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
				if (!vcr_comp_add_audio_data((unsigned char*)buf2, len2))
				{
					print_error(
						"Audio output failure!\nA call to addAudioData() (AVIStreamWrite) failed.\nPerhaps you ran out of memory?");
					vcr_stop_capture();
				}
			}
			sound_buf_pos = 0;
		}
	}

	if (len > 0)
	{
		if ((unsigned int)(sound_buf_pos + len) > SOUND_BUF_SIZE * sizeof(char))
		{
			MessageBox(nullptr, "Fatal error", "Sound buffer overflow", MB_ICONERROR);
			printf("SOUND BUFFER OVERFLOW\n");
			return;
		}
#ifdef _DEBUG
		else
		{
			const long double pro = (long double)(sound_buf_pos + len) * 100 / (
				SOUND_BUF_SIZE * sizeof(char));
			if (pro > 75) printf("---!!!---");
			printf("sound buffer: %.2Lf%%\n", pro);
		}
#endif
		memcpy(sound_buf + sound_buf_pos, (char*)buf, len);
		sound_buf_pos += len;
		m_audio_frame += ((len / 4) / (long double)m_audio_freq) *
			get_vis_per_second(ROM_HEADER.Country_code);
	}
}

// calculates how long the audio data will last
float get_percent_of_frame(const int ai_len, const int audio_freq, const int audio_bitrate)
{
	const int limit = (int)get_vis_per_second(ROM_HEADER.Country_code);
	const float vi_len = 1.f / (float)limit; //how much seconds one VI lasts
	const float time = (float)(ai_len * 8) / ((float)audio_freq * 2.f * (float)
		audio_bitrate); //how long the buffer can play for
	return time / vi_len; //ratio
}

void vcr_ai_len_changed()
{
	const auto p = reinterpret_cast<short*>((char*)rdram + (ai_register.ai_dram_addr
		& 0xFFFFFF));
	const auto buf = (char*)p;
	const int ai_len = (int)ai_register.ai_len;
	aiLenChanged();

	// hack - mupen64 updates bitrate after calling aiDacrateChanged
	m_audio_bitrate = (int)ai_register.ai_bitrate + 1;

	if (m_capture == 0)
		return;

	if (capture_with_ffmpeg)
	{
		capture_manager->WriteAudioFrame(buf, ai_len);
		return;
	}

	if (ai_len > 0)
	{
		const int len = ai_len;
		const int write_size = 2 * m_audio_freq;
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

		if (Config.synchronization_mode == vcr_sync_video_sndrop || Config.
			synchronization_mode == vcr_sync_none)
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

			long double desync = m_video_frame - m_audio_frame;

			if (Config.synchronization_mode == vcr_sync_none) // HACK
				desync = 0;

			if (desync > 1.0)
			{
				printf(
					"[VCR]: Correcting for A/V desynchronization of %+Lf frames\n",
					desync);
				int len3 = (int)(m_audio_freq / (long double)get_vis_per_second(ROM_HEADER.Country_code)) * (
					int)desync;
				len3 <<= 2;

				const int empty_size = len3 > write_size ? write_size : len3;

				for (int i = 0; i < empty_size; i += 4)
					*reinterpret_cast<long*>(sound_buf_empty + i) = last_sound;

				while (len3 > write_size)
				{
					write_sound(sound_buf_empty, write_size, m_audio_freq, write_size,
					           FALSE);
					len3 -= write_size;
				}
				write_sound(sound_buf_empty, len3, m_audio_freq, write_size, FALSE);
			} else if (desync <= -10.0)
			{
				printf(
					"[VCR]: Waiting from A/V desynchronization of %+Lf frames\n",
					desync);
			}
		}

		write_sound(buf, len, m_audio_freq, write_size, FALSE);

		last_sound = *(reinterpret_cast<long*>(buf + len) - 1);
	}
}

void update_title_bar_capture(const char* filename)
{
	// TODO: reimplement but less ugly
}

bool vcr_start_capture(const char* path, const bool show_codec_dialog)
{
	extern BOOL emu_paused;
	const BOOL was_paused = emu_paused;
	if (!emu_paused)
	{
		pause_emu(TRUE);
	}

	if (readScreen == nullptr)
	{
		printf("readScreen not implemented by graphics plugin. Falling back...\n");
		readScreen = vcrcomp_internal_read_screen;
	}

	memset(sound_buf_empty, 0, std::size(sound_buf_empty));
	memset(sound_buf, 0, std::size(sound_buf));
	last_sound = 0;
	m_video_frame = 0.0;
	m_audio_frame = 0.0;

	// fill in window size at avi start, which can't change
	// scrap whatever was written there even if window didnt change, for safety
	vcrcomp_window_info = {0};
	get_window_info(main_hwnd, vcrcomp_window_info);
	const long width = vcrcomp_window_info.width & ~3;
	const long height = vcrcomp_window_info.height & ~3;

	vcr_comp_start_file(path, width, height, (int)get_vis_per_second(ROM_HEADER.Country_code), show_codec_dialog);
	m_capture = 1;
	capture_with_ffmpeg = false;
	strncpy(avi_file_name, path, PATH_MAX);
	Config.avi_capture_path = path;


	// toolbar could get captured in AVI, so we disable it
	toolbar_set_visibility(0);
	update_title_bar_capture(avi_file_name);
	enable_emulation_menu_items(TRUE);
	SetWindowLong(main_hwnd, GWL_STYLE, GetWindowLong(main_hwnd, GWL_STYLE) & ~WS_MINIMIZEBOX);
	// we apply WS_EX_LAYERED to fix off-screen blitting (off-screen window portions are not included otherwise)
	SetWindowLong(main_hwnd, GWL_EXSTYLE, GetWindowLong(main_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);

	if (!was_paused || (m_task == e_task::playback || m_task == e_task::start_playback || m_task
		== e_task::start_playback_from_snapshot))
	{
		resume_emu(TRUE);
	}

	printf("[VCR]: Starting capture...\n");

	return true;
}

/// <summary>
/// Start capture using ffmpeg, if this function fails, manager (process and pipes) isn't created and error is returned.
/// </summary>
/// <param name="output_name">name of video file output</param>
/// <param name="arguments">additional ffmpeg params (output stream only)</param>
/// <returns></returns>
int vcr_start_ffmpeg_capture(const std::string& output_name,
                           const std::string& arguments)
{
	if (!emu_launched) return INIT_EMU_NOT_LAUNCHED;
	if (capture_manager != nullptr)
	{
#ifdef _DEBUG
		printf(
			"[VCR] Attempted to start ffmpeg capture when it was already started\n");
#endif
		return INIT_ALREADY_RUNNING;
	}
	t_window_info sInfo{};
	get_window_info(main_hwnd, sInfo);

	init_read_screen_ffmpeg(sInfo);
	capture_manager = std::make_unique<FFmpegManager>(
		sInfo.width, sInfo.height, get_vis_per_second(ROM_HEADER.Country_code), m_audio_freq,
		arguments + " " + output_name);

	const auto err = capture_manager->initError;
	if (err != INIT_SUCCESS)
		capture_manager.reset();
	else
	{
		update_title_bar_capture(output_name.data());
		m_capture = 1;
		capture_with_ffmpeg = true;
	}
#ifdef _DEBUG
	if (err == INIT_SUCCESS)
		printf("[VCR] ffmpeg capture started\n");
	else
		printf("[VCR] Could not start ffmpeg capture\n");
#endif
	return err;
}

void vcr_stop_ffmpeg_capture()
{
	if (capture_manager == nullptr) return; // no error code but its no big deal
	m_capture = 0;
	//this must be first in case object is being destroyed and other thread still sees m_capture=1
	capture_manager.reset();
	//apparently closing the pipes is enough to tell ffmpeg the movie ended.
#ifdef _DEBUG
	printf("[VCR] ffmpeg capture stopped\n");
#endif
}

void
vcr_toggle_read_only()
{
	vcr_set_read_only(!m_read_only);

	statusbar_post_text(m_read_only ? "Read" : "Read-write");
}

int vcr_stop_capture()
{
	if (capture_with_ffmpeg)
	{
		vcr_stop_ffmpeg_capture();
		return 0;
	}

	m_capture = 0;
	write_sound(nullptr, 0, m_audio_freq, m_audio_freq * 2, TRUE);

	// re-enable the toolbar (m_capture==0 causes this call to do that)
	// check previous update_toolbar_visibility call
	toolbar_set_visibility(Config.is_toolbar_enabled);

	SetWindowPos(main_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	reset_titlebar();
	SetWindowLong(main_hwnd, GWL_STYLE, GetWindowLong(main_hwnd, GWL_STYLE) | WS_MINIMIZEBOX);
	// we remove WS_EX_LAYERED again, because dwm sucks at dealing with layered top-level windows
	SetWindowLong(main_hwnd, GWL_EXSTYLE, GetWindowLong(main_hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);

	vcr_comp_finish_file(0);
	avi_increment = 0;
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

	if (vcr_is_recording())
	{
		std::string text = std::format("{} ({}) ", m_current_vi, vcr_current_sample);
		statusbar_post_text(text + input_string);
		statusbar_post_text(std::format("{} rr", vcr_movie_header.rerecord_count), 1);
	}

	if (vcr_is_playing())
	{
		std::string text = std::format("{} / {} ({} / {}) ", m_current_vi, vcr_get_length_v_is(), vcr_current_sample, vcr_get_length_samples());
		statusbar_post_text(text + input_string);
	}

	if (!vcr_is_active())
	{
		statusbar_post_text(input_string);
	}
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////// Recent Movies //////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void vcr_recent_movies_build(const int32_t reset)
{
	const HMENU h_menu = GetMenu(main_hwnd);

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

void vcr_recent_movies_add(const std::string& path)
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

int32_t vcr_recent_movies_play(const uint16_t menu_item_id)
{
	if (const int index = menu_item_id - ID_RECENTMOVIES_FIRST; index >= 0 && (unsigned int) index < Config.recent_movie_paths.size())
	{
		vcr_set_read_only(TRUE);
		return vcr_start_playback(Config.recent_movie_paths[index], false);
	}
	return 0;
}
