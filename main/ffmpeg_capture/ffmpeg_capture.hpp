#include <string>
#include <thread>
#include <queue>
#include <mutex>

#include "vcr_compress.h"

#ifdef _DEBUG
#define FFMPEG_BENCHMARK
#endif

const std::string default_options = "out.mp4";

//format signed 16 bit samples little endian
//rate taken from m_audioFreq
//two channels (always like that on n64)
#define audioOptions " -thread_queue_size 64 -f s16le -ar %d -ac 2 -channel_layout stereo "

// rawvideo demuxer,
// video size and framerate prepared in manager constructor when values are known
// pixel format is bgr24, because that's what windows uses
#define videoOptions " -thread_queue_size 4 -f rawvideo -video_size %dx%d -framerate %d -pixel_format bgr24 "
const char baseOptions[] = videoOptions "-i \\\\.\\pipe\\mupenvideo"
	audioOptions "-i \\\\.\\pipe\\mupenaudio ";

void init_read_screen_ffmpeg(const t_window_info& info);
void ffmpeg_cleanup();

enum initErrors
{
	UNKNOWN,
	INIT_SUCCESS,
	INIT_CREATEPROCESS_ERROR,
	INIT_PIPE_ERROR,
	INIT_ALREADY_RUNNING,
	INIT_EMU_NOT_LAUNCHED,
};

initErrors InitFFMPEGTest();

class FFmpegManager
{
public:
	/// <summary>
	/// Object used to communicate with ffmpeg process
	/// </summary>
	/// <param name="videoX">input video resolution</param>
	/// <param name="videoY">input video resolution</param>
	/// <param name="framerate">framerate (depends whether PAL or not)</param>
	/// <param name="cmdOptions">additional ffmpeg options (compression, output name, effects and shit)</param>
	FFmpegManager(unsigned videoX, unsigned videoY, unsigned framerate,
	              unsigned audiorate,
	              std::string cmdOptions = default_options);

	~FFmpegManager();

	int WriteVideoFrame(unsigned char* buffer, unsigned bufferSize);

	int WriteAudioFrame(char* buffer, unsigned bufferSize);

	void WriteAudioThread();
	void WriteVideoThread();

	// don't copy this object (why would you want to???)
	FFmpegManager(const FFmpegManager&) = delete;
	FFmpegManager& operator=(const FFmpegManager&) = delete;

	initErrors initError{};
	unsigned videoX, videoY;

private:
	int WritePipe(HANDLE pipe, char* buffer, unsigned bufferSize);

	int _WriteAudioFrame(char* buffer, unsigned bufferSize); //real writer
	int _WriteVideoFrame(unsigned char* buffer, unsigned bufferSize);

	STARTUPINFO si{};
	PROCESS_INFORMATION pi{};
	HANDLE videoPipe{};
	HANDLE audioPipe{};

	unsigned int audiorate{}; //bytes of audio needed to play 1 frame
	char* silenceBuffer{};

	bool stopThread = false;
	// (following comment functionality is disabled now) if you REALLY want to stop both threads, clear their queues. Otherwise this just tells thread to stop waiting for new stuff

	std::thread audioThread;
	std::mutex audioQueueMutex{};
	std::queue<std::vector<char>> audioQueue;
	// queue for sound frames to write to ffmpeg

	std::thread videoThread;
	std::mutex videoQueueMutex{};
	std::queue<std::vector<unsigned char>> videoQueue;
	// queue for sound frames to write to ffmpeg

	bool lastWriteWasVideo{};
};
