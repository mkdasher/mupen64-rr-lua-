// manager_win.cpp
// windows definition of the manager class
#include <string>
#include <queue>
#include <functional>
//#include <format>
#include <Windows.h>

#include "ffmpeg_capture.hpp"

FFmpegManager::FFmpegManager(unsigned videoX, unsigned videoY,
                             unsigned framerate, unsigned audiorate,
                             std::string cmdOptions) : videoX{videoX},
	videoY{videoY}, audiorate{audiorate},
	audioThread{&FFmpegManager::WriteAudioThread, this},
	// non static member, pass the hidden "this"
	videoThread{&FFmpegManager::WriteVideoThread, this}
{
	// partially copied from msdn
#ifdef _DEBUG
	{
		char buf[MAX_PATH];
		GetCurrentDirectory(sizeof(buf), buf);
		printf("Looking for ffmpeg.exe in %s\n", buf);
	}
#endif
	si.cb = sizeof(si);

	// Create named pipes
	//@TODO: calculate good bufsize? how
	unsigned int BUFSIZEvideo = 2048; //one frame size
	unsigned int BUFSIZEaudio = audiorate * 8; //bytes per second, maybe good

	videoPipe = CreateNamedPipe(
		"\\\\.\\pipe\\mupenvideo", // pipe name 
		PIPE_ACCESS_OUTBOUND, // read/write access 
		PIPE_TYPE_BYTE | // message type pipe 
		PIPE_READMODE_BYTE | // message-read mode 
		PIPE_WAIT, // blocking mode
		PIPE_UNLIMITED_INSTANCES, // max. instances  
		BUFSIZEvideo, // output buffer size 
		0, // input buffer size 
		0, // client time-out 
		NULL); // default security attribute 

	if (!videoPipe)
	{
		initError = INIT_PIPE_ERROR;
		return;
	}

	audioPipe = CreateNamedPipe(
		"\\\\.\\pipe\\mupenaudio", // pipe name 
		PIPE_ACCESS_OUTBOUND, // read/write access 
		PIPE_TYPE_BYTE | // message type pipe 
		PIPE_READMODE_BYTE | // message-read mode 
		PIPE_WAIT, // blocking mode
		PIPE_UNLIMITED_INSTANCES, // max. instances  
		BUFSIZEaudio, // output buffer size 
		0, // input buffer size 
		0, // client time-out 
		NULL);

	if (!audioPipe)
	{
		CloseHandle(videoPipe);
		initError = INIT_PIPE_ERROR;
		return;
	}

	// construct commandline
	// when micrisoft fixes c++20 this will use std::format instead
	{
		char buf[256];
		//@TODO: set audio format
		snprintf(buf, sizeof(buf), baseOptions, videoX, videoY, framerate,
		         audiorate);

		//prepend
		cmdOptions = buf + cmdOptions;
	}

	// Start the child process. 
	if (!CreateProcess("ffmpeg/bin/ffmpeg.exe",
	                   cmdOptions.data(), //non-const
	                   NULL, // Process handle not inheritable
	                   NULL, // Thread handle not inheritable
	                   FALSE, // Set handle inheritance to FALSE
	                   0, // No creation flags
	                   NULL, // Use parent's environment block
	                   NULL, // Use parent's starting directory 
	                   &si, // Pointer to STARTUPINFO structure
	                   &pi) // Pointer to PROCESS_INFORMATION structure
	)
	{
		MessageBox(
			NULL, "Could not start ffmpeg process! Does ffmpeg exist on disk?",
			"Error", MB_ICONERROR);
		printf("CreateProcess failed (%d).\n", GetLastError());
		CloseHandle(videoPipe);
		CloseHandle(audioPipe);
		initError = INIT_CREATEPROCESS_ERROR;
		return;
	}

	silenceBuffer = (char*)calloc(audiorate, 1); // I like old C functions more

	Sleep(500);
	// So basically, it's impossible to tell when ffmpeg has opened the pipes (only one of them at first)
	// and during this time mupen would be trying to use the pipes and stockpile audio and video in queue pointlessly.
	// Of course this is merely an estimate, but helps. 

	initError = INIT_SUCCESS;
}

FFmpegManager::~FFmpegManager()
{
	stopThread = true; // stop threads
	DisconnectNamedPipe(videoPipe);
	DisconnectNamedPipe(audioPipe);
	//TerminateProcess(pi.hProcess, 0);
	WaitForSingleObject(pi.hProcess, INFINITE);
	// I don't really care if the process is a zombie at this point


	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);


	audioThread.join(); // wait for it to return
	videoThread.join();

	free(silenceBuffer);
	ffmpeg_cleanup();
	// free buffer used by readscreen (starting two captures at once would break this way because buffer is global)
}

int FFmpegManager::_WriteVideoFrame(unsigned char* buffer, unsigned bufferSize)
{
	//printf("writing video frame\n");
	return WritePipe(videoPipe, (char*)buffer, bufferSize);
}

int FFmpegManager::_WriteAudioFrame(char* buffer, unsigned bufferSize)
//samples are signed
{
	//printf("writing audio frame: %d len\n", bufferSize);
	return WritePipe(audioPipe, buffer, bufferSize);
}

// in reality this doesn't write to pipe, but places things to queue
int FFmpegManager::WriteAudioFrame(char* buffer, unsigned bufferSize)
{
	lastWriteWasVideo = false;
	//printf("Adding to audio queue\n");
	audioQueueMutex.lock();
	this->audioQueue.push({buffer, buffer + bufferSize});
	audioQueueMutex.unlock();
	//printf("audio adding finished\n");
	return 0;
}

int FFmpegManager::WriteVideoFrame(unsigned char* buffer, unsigned bufferSize)
{
	if (lastWriteWasVideo)
	{
		// write frame of silence (I don't know if there's a way to do this without buffer)
		//printf("Writing silence...\n");
		WriteAudioFrame(silenceBuffer, audiorate / 16);
	}
	lastWriteWasVideo = true;
	//printf("Adding to video queue\n");
	videoQueueMutex.lock();
	this->videoQueue.push({buffer, buffer + bufferSize});
	videoQueueMutex.unlock();
	//printf("adding video finished\n");
	return 0;
}


int FFmpegManager::WritePipe(HANDLE pipe, char* buffer, unsigned bufferSize)
{
	DWORD written{};
	auto res = WriteFile(pipe, buffer, bufferSize, &written, NULL);
	if (written != bufferSize or res != TRUE)
	{
		return GetLastError();
	}
	return 0;
}

void FFmpegManager::WriteAudioThread()
{
	// a special variable is used, since it's hard to guarantee that, for example, VCR_isCapturing() will be correct.
	// waits until the queue is empty, then stops (this is commented out)
	while (!this->stopThread) // || !this->audioQueue.empty()) 
	{
		if (!this->audioQueue.empty())
		{
			//printf("Writing audio (thread)\n");
			this->audioQueueMutex.lock();
			auto& frame = this->audioQueue.front();
			this->audioQueueMutex.unlock();
			if (!this->_WriteAudioFrame(frame.data(), frame.size()))
			// waits here
			{
				//printf(">writing audio success\n");
				this->audioQueueMutex.lock();
				this->audioQueue.pop(); //only remove when write succesfull
				this->audioQueueMutex.unlock();
			} else
			{
				printf(">>>>>>>>>>>>>>>writing audio fail, queue size: %d\n",
				       this->audioQueue.size());
			}
		} else
		{
			// sleep?
			Sleep(10);
		}
	}

	printf(">>>Remaining audio queue stuff: %d\n", this->audioQueue.size());
}

void FFmpegManager::WriteVideoThread()
{
	// a special variable is used, since it's hard to guarantee that, for example, VCR_isCapturing() will be correct.
	// waits until the queue is empty, then stops
	while (!this->stopThread) // || !this->audioQueue.empty())
	{
		if (!this->videoQueue.empty())
		{
			//printf("Writing video (thread)\n");
			this->videoQueueMutex.lock();
			auto& frame = this->videoQueue.front();
			this->videoQueueMutex.unlock();
			if (!this->_WriteVideoFrame(frame.data(), frame.size()))
			// waits here
			{
				// printf(">writing video success\n");
				this->videoQueueMutex.lock();
				this->videoQueue.pop(); //only remove when write succesfull
				this->videoQueueMutex.unlock();
			} else
			{
				printf(">>>>>>>>>>>>>>>writing video fail, queue size: %d\n",
				       this->videoQueue.size());
			}
		} else
		{
			// sleep?
			Sleep(10);
		}
	}

	printf(">>>Remaining video queue stuff: %d\n", this->videoQueue.size());
}


// test default constructor, also this can be externed to C
initErrors InitFFMPEGTest()
{
	FFmpegManager manager(100, 100, 60, 32000);
	return manager.initError;
}
