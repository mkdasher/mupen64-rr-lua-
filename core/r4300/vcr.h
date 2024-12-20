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
    uint8_t authorship_tag[4] = {0x4D, 0x55, 0x50, 0x4E};

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
    uint32_t magic;

    /**
     * \brief Currently <c>3</c>
     */
    uint32_t version;

    /**
     * \brief Movie creation time, used to correlate savestates to a movie
     */
    uint32_t uid;

    /**
     * \brief Amount of VIs in the movie
     */
    uint32_t length_vis;

    /**
     * \brief The low word of the amount of loaded states during recording.
     */
    uint32_t rerecord_count;

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
    uint32_t length_samples;

    /**
     * \brief What state the movie is expected to start from
     * \remarks vcr.h:32
     */
    uint16_t startFlags;

    uint16_t reserved2;

    /**
     * \brief Bitfield of flags for each controller. Sequence of present, mempak and rumble bits with per-controller values respectively (c1,c2,c3,c4,r1,...)
     */
    uint32_t controller_flags;

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
    uint32_t rom_crc1;

    /**
     * \brief The rom country of the rom the movie was recorded on
     */
    uint16_t rom_country;

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

enum class e_warp_modify_status
{
    none,
    warping,
};

/**
 * \brief The movie freeze buffer, which is used to store the movie (with only essential data) associated with a savestate inside the savestate.
 */
struct t_movie_freeze
{
    uint32_t size;
    uint32_t uid;
    uint32_t current_sample;
    uint32_t current_vi;
    uint32_t length_samples;
    std::vector<BUTTONS> input_buffer;
};

bool task_is_playback(e_task task);
bool task_is_recording(e_task task);

/**
 * \brief Notifies VCR engine about controller being polled
 * \param index The polled controller's index
 * \param input The controller's input data
 */
void vcr_on_controller_poll(int32_t index, BUTTONS* input);

/**
 * \brief Notifies VCR engine about a new VI
 */
void vcr_on_vi();

namespace VCR
{
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
    CoreResult parse_header(std::filesystem::path path, t_movie_header* header);

    /**
     * \brief Reads the inputs from a movie
     * \param path The movie's path
     * \param inputs The button collection to fill
     * \return The operation result
     */
    CoreResult read_movie_inputs(std::filesystem::path path, std::vector<BUTTONS>& inputs);

    /**
     * \brief Starts playing back a movie
     * \param path The movie's path
     * \return The operation result
     */
    CoreResult start_playback(std::filesystem::path path);

    /**
     * \brief Starts recording a movie
     * \param path The movie's path
     * \param flags Start flags, see MOVIE_START_FROM_X
     * \param author The movie author's name
     * \param description The movie's description
     * \return The operation result
     */
    CoreResult start_record(std::filesystem::path path, uint16_t flags, std::string author = "(no author)", std::string description = "(no description)");

    /**
     * \brief Replaces the author and description information of a movie
     * \param path The movie's path
     * \param author The movie author's name
     * \param description The movie's description
     * \return The operation result
     */
    CoreResult replace_author_info(const std::filesystem::path& path, const std::string& author, const std::string& description);

    /**
     * \brief Gets the completion status of the current seek operation.
     * If no seek operation is active, the first value is the current sample and the second one is SIZE_MAX.
     */
    std::pair<size_t, size_t> get_seek_completion();

    /**
     * \brief Begins seeking to a frame in the currently movie.
     * \param str A seek format string
     * \param pause_at_end Whether the emu should be paused when the seek operation ends
     * \return The operation result
     * \remarks When the seek operation completes, the SeekCompleted message will be sent
     *
     * Seek string format possibilities:
     *	"n" - Frame
     *	"+n", "-n" - Relative to current sample
     *	"^n" - Sample n from the end
     *
     */
    CoreResult begin_seek(std::wstring str, bool pause_at_end);

    /**
     * \brief Converts a freeze buffer into a movie, trying to reconstruct as much as possible
     * \param freeze The freeze buffer to convert
     * \param header The generated header
     * \param inputs The generated inputs
     * \return The operation result
     */
    CoreResult convert_freeze_buffer_to_movie(const t_movie_freeze& freeze, t_movie_header& header, std::vector<BUTTONS>& inputs);

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
    CoreResult unfreeze(t_movie_freeze freeze);

    /**
     * \brief Stops all running tasks
     * \return The operation result
     */
    CoreResult stop_all();

    /**
     * \brief Gets the current movie path. Only valid when task is not idle.
     */
    std::filesystem::path get_path();

    /**
     * \brief Gets the current task
     */
    e_task get_task();

    /**
	 * Gets the sample length of the current movie. If no movie is active, the function returns UINT32_MAX.
	 */
	uint32_t get_length_samples();

	/**
	 * Gets the VI length of the current movie. If no movie is active, the function returns UINT32_MAX.
	 */
	uint32_t get_length_vis();

	/**
	 * Gets the current VI in the current movie. If no movie is active, the function returns -1.
	 */
	int32_t get_current_vi();

    /**
     * Gets a copy of the current input buffer
     */
    std::vector<BUTTONS> get_inputs();

    /**
     * Begins a warp modification operation. A "warp modification operation" is the changing of sample data which is temporally behind the current sample.
     *
     * The VCR engine will find the last common sample between the current input buffer and the provided one.
     * Then, the closest savestate prior to that sample will be loaded and recording will be resumed with the modified inputs up to the sample the function was called at.
     *
     * This operation is long-running and status is reported via the WarpModifyStatusChanged message.
     * A successful warp modify operation can be detected by the status changing from warping to none with no errors inbetween.
     *
     * If the provided buffer is identical to the current input buffer (in both content and size), the operation will succeed with no seek.
     *
     * If the provided buffer is larger than the current input buffer and the first differing input is after the current sample, the operation will succeed with no seek.
     * The input buffer will be overwritten with the provided buffer and when the modified frames are reached in the future, they will be "applied" like in playback mode.
     *
     * If the provided buffer is smaller than the current input buffer, the VCR engine will seek to the last frame and otherwise perform the warp modification as normal.
     *
     * An empty input buffer will cause the operation to fail.
     *
     * \param inputs The input buffer to use.
     * \return The operation result
     */
    CoreResult begin_warp_modify(const std::vector<BUTTONS>& inputs);

    /**
     * Gets the warp modify status
     */
    e_warp_modify_status get_warp_modify_status();

    /**
	 * Gets the first differing frame when performing a warp modify operation.
	 * If no warp modify operation is active, the function returns SIZE_MAX.
	 */
	size_t get_warp_modify_first_difference_frame();

    /**
     * Gets the current seek savestate frames.
     * Keys are frame numbers, values are unimportant and abscence of a seek savestate at a frame is marked by the respective key not existing.
     */
    std::unordered_map<size_t, bool> get_seek_savestate_frames();

    /**
     * Gets whether a seek savestate exists at the specified frame.
     * The returned state changes when the <c>SeekSavestatesChanged</c> message is sent.
     */
    bool has_seek_savestate_at_frame(size_t frame);

    /**
     * HACK: The VCR engine can prevent the core from pausing. Gets whether the core should be allowed to pause.
     */
    bool allows_core_pause();
}

bool is_frame_skipped();
