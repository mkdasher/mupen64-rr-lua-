#pragma once

#include <functional>
#include <core/r4300/Plugin.hpp>

enum
{
	MOVIE_START_FROM_SNAPSHOT = (1 << 0),
	MOVIE_START_FROM_NOTHING = (1 << 1),
	MOVIE_START_FROM_EEPROM = (1 << 2),
	MOVIE_START_FROM_EXISTING_SNAPSHOT = (1 << 3)
};

#define CONTROLLER_X_PRESENT(x)	(1<<(x))
#define CONTROLLER_X_MEMPAK(x)	(1<<((x)+4))
#define CONTROLLER_X_RUMBLE(x)	(1<<((x)+8))

#pragma pack(push, 1)
/**
 * The movie flag bitfield for extended movies.  
 */
typedef union ExtendedMovieFlags
{
	uint8_t data;

	struct
	{
		/**
		 * Whether the movie was recorded with WiiVC mode enabled.
		 */
		bool wii_vc : 1;
		bool unused_1 : 1;
		bool unused_2 : 1;
		bool unused_3 : 1;
		bool unused_4 : 1;
		bool unused_5 : 1;
		bool unused_6 : 1;
		bool unused_7 : 1;
	};
} t_extended_movie_flags;

/**
 * Additional data for extended movies. Must be 32 bytes large.
 */
typedef struct ExtendedMovieData
{
	/**
	 * Special authorship information, such as the program which created the movie.
	 */
	uint8_t authorship_tag[4] = { 0x4D, 0x55, 0x50, 0x4E };

	/**
	 * Additional data regarding bruteforcing.
	 */
	uint32_t bruteforce_extra_data;

	/**
	 * The high word of the amount of loaded states during recording.
	 */
	uint32_t rerecord_count;

	uint64_t unused_1;
	uint64_t unused_2;
	uint32_t unused_3;
	
} t_extended_movie_data;

/**
 * \brief
 */
typedef struct MovieHeader
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
	 * \brief The low word of the amount of loaded states during recording.
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

	/**
	 * The extended format version. Old movies have it set to <c>0</c>.  
	 */
	uint8_t extended_version = 1;

	/**
	 * The extended movie flags. Only valid if <c>extended_version != 0</c>.
	 */
	t_extended_movie_flags extended_flags;

	/**
	 * \brief Amount of input samples in the movie, ideally equal to <c>(file_length - 1024) / 4</c>
	 */
	unsigned long length_samples;

	/**
	 * \brief What state the movie is expected to start from
	 * \remarks vcr.h:32
	 */
	unsigned short startFlags;

	unsigned short reserved2;

	/**
	 * \brief Bitfield of flags for each controller. Sequence of present, mempak and rumble bits with per-controller values respectively (c1,c2,c3,c4,r1,...)
	 */
	unsigned long controller_flags;

	/**
	 * The extended movie data. Only valid if <c>extended_version != 0</c>.
	 */
	t_extended_movie_data extended_data{};

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
	char author[222];

	/**
	 * \brief A description of what the movie is about as a UTF-8 string
	 */
	char description[256];
} t_movie_header;
#pragma pack(pop)

enum class e_task
{
	idle,
	start_recording_from_reset,
	start_recording_from_snapshot,
	start_recording_from_existing_snapshot,
	recording,
	start_playback_from_reset,
	start_playback_from_snapshot,
	playback
};


/**
 * \brief The movie freeze buffer, which is used to store the movie (with only essential data) associated with a savestate inside the savestate.
 */
struct t_movie_freeze
{
	unsigned long size;
	unsigned long uid;
	unsigned long current_sample;
	unsigned long current_vi;
	unsigned long length_samples;
	std::vector<BUTTONS> input_buffer;
};

bool task_is_playback(e_task task);
bool task_is_recording(e_task task);

/**
 * \brief Notifies VCR engine about controller being polled
 * \param index The polled controller's index
 * \param input The controller's input data
 */
void vcr_on_controller_poll(int index, BUTTONS* input);

/**
 * \brief Notifies VCR engine about a new VI
 */
void vcr_on_vi();

namespace VCR
{
	enum class Result
	{
		// The operation completed successfully
		Ok,
		
		// The provided data has an invalid format
		InvalidFormat,
		
		// The provided file is inaccessible or does not exist
		BadFile,
		
		// The user cancelled the operation
		Cancelled,
		
		// The controller configuration is invalid
		InvalidControllers,
		
		// The movie's savestate is missing or invalid
		InvalidSavestate,
		
		// The resulting frame is outside the bounds of the movie
		InvalidFrame,
		
		// There is no rom which matches this movie
		NoMatchingRom,
		
		// The callee is already performing another task
		Busy,
		
		// The VCR engine is idle, but must be active to complete this operation
		Idle,
		
		// The provided freeze buffer is not from the currently active movie
		NotFromThisMovie,
		
		// The movie's version is invalid
		InvalidVersion,
		
		// The movie's extended version is invalid
		InvalidExtendedVersion,
		
		// The operation requires a playback or recording task
		NeedsPlaybackOrRecording,
		
		// The provided start type is invalid.
		InvalidStartType,
	};

	/**
	 * \brief Initializes the VCR engine
	 */
	void init();

	/**
	 * \brief Parses a movie's header
	 * \param path The movie's path
	 * \param header The header to fill
	 * \return The operation result
	 */
	Result parse_header(std::filesystem::path path, t_movie_header* header);

	/**
	 * \brief Reads the inputs from a movie
	 * \param path The movie's path
	 * \param inputs The button collection to fill
	 * \return The operation result
	 */
	Result read_movie_inputs(std::filesystem::path path, std::vector<BUTTONS>& inputs);

	/**
 	* \brief Starts playing back a movie.
 	* \param path The movie's path
 	* \remarks Asynchronous function. Progress updates are received via the <c>VCR_StartPlaybackResult</c> message
 	*/
	void start_playback(std::filesystem::path path);

	/**
	 * \brief Starts recording a movie
	 * \param path The movie's path
	 * \param flags Start flags, see MOVIE_START_FROM_X
	 * \param author The movie author's name
	 * \param description The movie's description
	 * \return The operation result
	 */
	Result start_record(std::filesystem::path path, uint16_t flags, std::string author = "(no author)", std::string description = "(no description)");


	/**
	 * \brief Replaces the author and description information of a movie
	 * \param path The movie's path
	 * \param author The movie author's name
	 * \param description The movie's description
	 * \return The operation result
	 */
	Result replace_author_info(const std::filesystem::path& path, const std::string& author, const std::string& description);

	/**
	 * \brief Gets information about a seek operation
	 * \param frame The frame to seek to
	 * \param relative Whether the seek is relative to the current frame
	 * \return A pair containing whether the specified parameters allow a seek operation and the effective frame
	 */
	std::pair<bool, size_t> get_seek_info(int32_t frame, bool relative);

	/**
	 * \brief Gets the completion status of the current seek operation. If no seek operation is active, both pair values are 0.
	 */
	std::pair<size_t, size_t> get_seek_completion();

	/**
	 * \brief Begins seeking to a frame in the currently playing movie
	 * \param frame The frame to seek to
	 * \param relative Whether the seek is relative to the current frame
	 * \return The operation result
	 * \remarks When the seek operation completes, the SeekCompleted message will be sent
	 */
	Result begin_seek_to(int32_t frame, bool relative);

	/**
	 * \brief Converts a freeze buffer into a movie, trying to reconstruct as much as possible
	 * \param freeze The freeze buffer to convert
	 * \param header The generated header
	 * \param inputs The generated inputs
	 * \return The operation result
	 */
	Result convert_freeze_buffer_to_movie(const t_movie_freeze& freeze, t_movie_header& header, std::vector<BUTTONS>& inputs);

	/**
	 * \brief Stops the current seek operation
	 */
	void stop_seek();

	/**
	 * \brief Gets whether the VCR engine is currently performing a seek operation
	 */
	bool is_seeking();

	/**
	 * \brief Generates the current movie freeze buffer
	 * \return The movie freeze buffer, or nothing
	 */
	std::optional<t_movie_freeze> freeze();

	/**
	 * \brief Restores the movie from a freeze buffer
	 * \param freeze The freeze buffer
	 * \return The operation result
	 */
	Result unfreeze(t_movie_freeze freeze);

	/**
	 * \brief Stops all running tasks
	 * \return The operation result
	 */
	Result stop_all();
	
	/**
	 * \brief Gets the text representation of the last frame's inputs
	 */
	const char* get_input_text();

	/**
	 * \brief Gets the text representation of the VCR state's inputs
	 */
	const char* get_status_text();

	/**
	 * \brief Gets the current movie path. Only valid when task is not idle.
	 */
	std::filesystem::path get_path();

	/**
	 * \brief Gets the current task
	 */
	e_task get_task();
}

bool is_frame_skipped();
