#ifndef VCR_H
#define VCR_H

# include <Windows.h>

#include "plugin.hpp"

#ifdef __cplusplus	//don't include cpp headers when .c compilation unit includes the file
#include <string>	//(also done later for ffmpeg functions)
#include <vector>
#endif

enum {
	success = (0),
	wrong_format = (1),
	wrong_version = (2),
	file_not_found = (3),
	not_from_this_movie = (4),
	not_from_a_movie = (5),
	invalid_frame = (6),
	unknown_error = (7)
};

enum {
	vcr_playback_success = (0),
	vcr_playback_error = (-1),
	vcr_playback_savestate_missing = (-2),
	vcr_playback_file_busy = (-3),
	vcr_playback_incompatible = (-4)
};

enum {
	movie_start_from_snapshot = (1<<0),
	movie_start_from_nothing = (1<<1),
	movie_start_from_eeprom = (1<<2),
	movie_start_from_existing_snapshot = (1<<3)
};

#define CONTROLLER_X_PRESENT(x)	(1<<(x))
enum {
	controller_1_present = (1<<0),
	controller_2_present = (1<<1),
	controller_3_present = (1<<2),
	controller_4_present = (1<<3)
};

#define CONTROLLER_X_MEMPAK(x)	(1<<((x)+4))
enum {
	controller_1_mempak = (1<<4),
	controller_2_mempak = (1<<5),
	controller_3_mempak = (1<<6),
	controller_4_mempak = (1<<7)
};

#define CONTROLLER_X_RUMBLE(x)	(1<<((x)+8))
enum {
	controller_1_rumble = (1<<8),
	controller_2_rumble = (1<<9),
	controller_3_rumble = (1<<10),
	controller_4_rumble = (1<<11)
};

enum {
	movie_author_data_size = (222),
	movie_description_data_size = (256)
};
#define MOVIE_MAX_METADATA_SIZE (MOVIE_DESCRIPTION_DATA_SIZE > MOVIE_AUTHOR_DATA_SIZE ? MOVIE_DESCRIPTION_DATA_SIZE : MOVIE_AUTHOR_DATA_SIZE)

enum {
	vcr_sync_audio_dupl = 0,
	vcr_sync_video_sndrop = 1,
	vcr_sync_none = 2
};

/*
	(0x0001)	Directional Right
	(0x0002)	Directional Left
	(0x0004)	Directional Down
	(0x0008)	Directional Up
	(0x0010)	Start
	(0x0020)	Z
	(0x0040)	B
	(0x0080)	A
	(0x0100)	C Right
	(0x0200)	C Left
	(0x0400)	C Down
	(0x0800)	C Up
	(0x1000)	R
	(0x2000)	L
	(0x00FF0000) Analog X
	(0xFF000000) Analog Y
*/

extern void vcr_update_screen();
extern void vcr_ai_dacrate_changed(system_type type);
extern void vcr_ai_len_changed();

extern BOOL vcr_is_active();
extern BOOL vcr_is_idle(); // not the same as !isActive()
extern BOOL vcr_is_starting();
extern BOOL vcr_is_starting_and_just_restarted();
extern BOOL vcr_is_playing();
extern BOOL vcr_is_recording();
extern BOOL vcr_is_capturing();
extern BOOL vcr_get_read_only();
extern bool vcr_is_looping();
extern bool vcr_is_restarting();
extern void vcr_set_read_only(BOOL val);
extern unsigned long vcr_get_length_v_is();
extern unsigned long vcr_get_length_samples();
extern void vcr_set_length_v_is(unsigned long val);
extern void vcr_toggle_read_only();


//ffmpeg
#ifdef __cplusplus
int vcr_start_ffmpeg_capture(const std::string& output_name,
                           const std::string& arguments);
void vcr_stop_ffmpeg_capture();
#endif

/**
 * \brief Stops and finalizes all current and pending VCR operations, such as pending or active movie recording
 */
void vcr_stop_all();

extern void print_error(const char*);

/**
 * \brief Updates the statusbar with the current VCR state
 */
void vcr_update_statusbar();

/**
 * \brief Clears all save data related to the current rom, such as SRAM, EEP and mempak
 */
void vcr_clear_save_data();

/**
 * \brief Starts an AVI capture
 * \param path The path to the AVI output file
 * \param show_codec_dialog Whether the user should be presented with a dialog to pick the capture codec
 * \return Whether the operation succeded
 */
bool vcr_start_capture(const char* path, bool show_codec_dialog);

/**
 * \brief Stops the current capture
 * \return The operation's status code
 */
int vcr_stop_capture();

/**
 * \brief Notifies VCR engine about controller being polled
 * \param index The polled controller's index
 * \param input The controller's input data
 */
void vcr_on_controller_poll(int index, BUTTONS* input);

/**
 * \brief Starts recording a movie
 * \param path The path to record the movie to
 * \param start_flag The movie's start flag
 * \return The operation's status code
 */
int vcr_start_record(const std::filesystem::path& path, uint16_t start_flag);

/**
 * \brief Stops recording a movie
 * \return The operation's status code
 */
int vcr_stop_record();

/**
 * \brief Starts playing back a movie
 * \param path The movie's path
 * \param restarting TBD
 * \return The operation's status code
 */
int vcr_start_playback(const std::filesystem::path& path, bool restarting);

/**
 * \brief Stops playing back a movie
 * \param bypass_loop_setting Whether the movie loop setting should be ignored
 * \return The operation's status code
 */
int vcr_stop_playback(bool bypass_loop_setting);


void vcr_recent_movies_build(int32_t reset = 0);
void vcr_recent_movies_add(const std::string& path);
int32_t vcr_recent_movies_play(uint16_t menu_item_id);

#pragma pack(push, 1)
/**
 * \brief
 */
typedef struct
{
	/**
	 * \brief <c>M64\0x1a</c>
	 */
	unsigned long magic;

	/**
	 * \brief Currently <c>3</c>
	 */
	unsigned long version;

	/**
	 * \brief Movie creation time, used to correlate savestates to a movie
	 */
	unsigned long uid;

	/**
	 * \brief Amount of VIs in the movie
	 */
	unsigned long length_vis;

	/**
	 * \brief Amount of loaded states during recording
	 */
	unsigned long rerecord_count;

	/**
	 * \brief The VIs per second of the rom this movie was recorded on
	 * \remarks (unused)
	 * \remarks This field makes no sense, as VI/s are already known via ROM cc
	 */
	unsigned char vis_per_second;

	/**
	 * \brief The amount of controllers connected during recording
	 * \remarks (pretty much unused)
	 * \remarks This field makes no sense, as this value can already be figured out by checking controller flags post-load
	 */
	unsigned char num_controllers;

	unsigned short reserved1;

	/**
	 * \brief Amount of input samples in the movie, ideally equal to <c>(file_length - 1024) / 4</c>
	 */
	unsigned long length_samples;

	/**
	 * \brief What state the movie is expected to start from
	 * \remarks vcr.h:32
	 */
	unsigned short start_flags;

	unsigned short reserved2;

	/**
	 * \brief Bitfield of flags for each controller. Sequence of present, mempak and rumble bits with per-controller values respectively (c1,c2,c3,c4,r1,...)
	 */
	unsigned long controller_flags;

	unsigned long reserved_flags[8];

	/**
	 * \brief The name of the movie's author
	 * \remarks Used in .rec (version 2)
	 */
	char old_author_info[48];

	/**
	 * \brief A description of what the movie is about
	 * \remarks Used in .rec (version 2)
	 */
	char old_description[80];

	/**
	 * \brief The internal name of the rom the movie was recorded on
	 */
	char rom_name[32];

	/**
	 * \brief The primary checksum of the rom the movie was recorded on
	 */
	unsigned long rom_crc1;

	/**
	 * \brief The rom country of the rom the movie was recorded on
	 */
	unsigned short rom_country;

	char reserved_bytes[56];

	/**
	 * \brief The name of the video plugin the movie was recorded with, as specified by the plugin
	 */
	char video_plugin_name[64];

	/**
	 * \brief The name of the audio plugin the movie was recorded with, as specified by the plugin
	 */
	char audio_plugin_name[64];

	/**
	 * \brief The name of the input plugin the movie was recorded with, as specified by the plugin
	 */
	char input_plugin_name[64];

	/**
	 * \brief The name of the RSP plugin the movie was recorded with, as specified by the plugin
	 */
	char rsp_plugin_name[64];

	/**
	 * \brief The name of the movie's author as a UTF-8 string
	 */
	char author[movie_author_data_size];

	/**
	 * \brief A description of what the movie is about as a UTF-8 string
	 */
	char description[movie_description_data_size];
} t_movie_header;
#pragma pack(pop)

typedef struct
{
	uint64_t input_size;
	uint64_t uid;
	uint64_t current_sample;
	uint64_t current_vi;
	uint64_t length_samples;
} t_vcr_freeze;

enum class e_task
{
	idle,
	start_recording,
	start_recording_from_snapshot,
	start_recording_from_existing_snapshot,
	recording,
	start_playback,
	start_playback_from_snapshot,
	playback
};

extern e_task m_task;

inline bool is_task_playback(e_task task)
{
	return task == e_task::start_playback || task == e_task::start_playback_from_snapshot || task == e_task::playback;
}
inline bool is_task_recording(e_task task)
{
	return task == e_task::start_recording || task == e_task::start_recording_from_snapshot || task == e_task::start_recording_from_existing_snapshot || task == e_task::recording;
}

/**
 * \brief Parses a buffer into a movie header with the current format
 * \param buffer The buffer to read from
 * \param header The target header
 * \return Whether the operation succeeded
 */
bool vcr_parse_header(std::vector<uint8_t>& buffer, t_movie_header* header);


/**
 * \brief Restores VCR state from a savestate
 * \param freeze The st's freeze buffer
 * \param input_buffer A vector of inputs to overwrite the currently playing movie with
 * \return Whether the operation succeeded
 */
bool vcr_restore(t_vcr_freeze freeze, std::vector<BUTTONS>& input_buffer);

extern char vcr_lastpath[MAX_PATH];
extern uint64_t screen_updates;
extern std::vector<BUTTONS> movie_inputs;
extern std::filesystem::path movie_path;
extern uint64_t vcr_current_sample;
extern uint64_t vcr_current_vi;
extern t_movie_header vcr_movie_header;

#endif // VCR_H
