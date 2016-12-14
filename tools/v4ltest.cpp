#include <linux/videodev.h>
#include <sys/ioctl.h>

int main(void)
{
	int dev = 0;
	struct v4l2_capability capabilities;

	ioctl(dev, VIDIOC_QUERYCAP, &capabilities);

	return 0;
}
