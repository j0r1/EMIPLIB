#include <sys/soundcard.h>
#include <sys/ioctl.h>

int main(void)
{
	int dev = 0;
	int val = 0;

	ioctl(dev, SNDCTL_DSP_SETFRAGMENT, &val);

	return 0;
}
