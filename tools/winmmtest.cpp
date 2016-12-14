#include <windows.h>
#ifndef _WIN32_WCE
	#include <mmsystem.h>
#endif // _WIN32_WCE

int main(void)
{
	WAVEFORMATEX format;

	format.wFormatTag = WAVE_FORMAT_PCM;

	return 0;
}

