#include "AVIEncoder.h"

#include <view/helpers/IOHelpers.h>
#include <view/gui/Main.h>

bool AVIEncoder::start(Params params)
{
	if (!splitting)
	{
		video_params = params;
	}
	avi_opened = true;
	AVIFileSize = 0;
	frame = 0;
	infoHeader.biSize = sizeof(BITMAPINFOHEADER);
	infoHeader.biWidth = params.width;
	infoHeader.biHeight = params.height;
	infoHeader.biPlanes = 1;
	infoHeader.biBitCount = 24;
	infoHeader.biCompression = BI_RGB;
	infoHeader.biSizeImage = params.width * params.height * 3;
	infoHeader.biXPelsPerMeter = 0;
	infoHeader.biYPelsPerMeter = 0;
	infoHeader.biClrUsed = 0;
	infoHeader.biClrImportant = 0;

	AVIFileInit();
	AVIFileOpen(&avi_file, params.path.string().c_str(), OF_WRITE | OF_CREATE,
	            NULL);

	ZeroMemory(&video_stream_header, sizeof(AVISTREAMINFO));
	video_stream_header.fccType = streamtypeVIDEO;
	video_stream_header.dwScale = 1;
	video_stream_header.dwRate = params.fps;
	video_stream_header.dwSuggestedBufferSize = 0;
	AVIFileCreateStream(avi_file, &video_stream, &video_stream_header);

	if (params.ask_for_encoding_settings)
	{
		if (!AVISaveOptions(g_main_hwnd, 0, 1, &video_stream, &avi_options))
		{
			printf("[AVIEncoder] Failed to save options\n");
			return false;
		}
	} else
	{
		if (!load_options())
		{
			printf("[AVIEncoder] Failed to load options\n");
			return false;
		}
	}

	save_options();
	AVIMakeCompressedStream(&compressed_video_stream, video_stream,
	                        avi_options, NULL);
	AVIStreamSetFormat(compressed_video_stream, 0, &infoHeader,
	                   infoHeader.biSize + infoHeader.biClrUsed * sizeof(
		                   RGBQUAD));

	sample = 0;
	sound_format.wFormatTag = WAVE_FORMAT_PCM;
	sound_format.nChannels = 2;
	sound_format.nSamplesPerSec = 44100;
	sound_format.nAvgBytesPerSec = 44100 * (2 * 16 / 8);
	sound_format.nBlockAlign = 2 * 16 / 8;
	sound_format.wBitsPerSample = 16;
	sound_format.cbSize = 0;

	ZeroMemory(&sound_stream_header, sizeof(AVISTREAMINFO));
	sound_stream_header.fccType = streamtypeAUDIO;
	sound_stream_header.dwQuality = (DWORD)-1;
	sound_stream_header.dwScale = sound_format.nBlockAlign;
	sound_stream_header.dwInitialFrames = 1;
	sound_stream_header.dwRate = sound_format.nAvgBytesPerSec;
	sound_stream_header.dwSampleSize = sound_format.nBlockAlign;
	AVIFileCreateStream(avi_file, &sound_stream, &sound_stream_header);
	AVIStreamSetFormat(sound_stream, 0, &sound_format, sizeof(WAVEFORMATEX));
	return true;
}

bool AVIEncoder::stop()
{
	AVIStreamClose(compressed_video_stream);
	AVIStreamRelease(video_stream);
	AVIStreamClose(sound_stream);
	AVIFileClose(avi_file);
	AVIFileExit();

	avi_opened = false;
	return true;
}

bool AVIEncoder::append_video(uint8_t* image)
{
	if (AVIFileSize > max_avi_size)
	{
		// If AVI file gets too big, it will corrupt or crash. Since this is a limitation the caller shouldn't care about, we split the recording silently.
		// When splitting, filenames follow this pattern: <fname> <n>.avi
		Encoder::Params new_params = video_params;
		new_params.path = with_name(new_params.path,
		                            get_name(video_params.path) + " " +
		                            std::to_string(++splits));

		splitting = true;
		stop();
		start(new_params);
		splitting = false;
	}
	LONG written_len;
	BOOL ret = AVIStreamWrite(compressed_video_stream, frame++, 1, image,
	                          infoHeader.biSizeImage, AVIIF_KEYFRAME, NULL,
	                          &written_len);
	AVIFileSize += written_len;
	return !ret;
}

bool AVIEncoder::append_audio(uint8_t* audio, size_t length)
{
	BOOL ok = (0 == AVIStreamWrite(sound_stream, sample,
	                               length / sound_format.nBlockAlign, audio, length, 0,
	                               NULL, NULL));
	sample += length / sound_format.nBlockAlign;
	AVIFileSize += length;
	return ok;
}

bool AVIEncoder::save_options()
{
	FILE* f = fopen("avi.cfg", "wb");
	fwrite(avi_options, sizeof(AVICOMPRESSOPTIONS), 1, f);
	fwrite(avi_options->lpParms, avi_options->cbParms, 1, f);
	fclose(f);
	return true;
}

bool AVIEncoder::load_options()
{
	FILE* f = fopen("avi.cfg", "rb");

	if (!f)
		return false;

	fseek(f, 0, SEEK_END);

	// Too small...
	if (ftell(f) < sizeof(AVICOMPRESSOPTIONS))
		goto error;

	fseek(f, 0, SEEK_SET);

	fread(avi_options, sizeof(AVICOMPRESSOPTIONS), 1, f);

	{
		void* moreOptions = malloc(avi_options->cbParms);
		fread(moreOptions, avi_options->cbParms, 1, f);
		avi_options->lpParms = moreOptions;
	}

	fclose(f);
	return true;

error:
	fclose(f);
	return false;
}
