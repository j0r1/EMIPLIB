#include <jrtplib3/rtpsecuresession.h>
#include <jrtplib3/rtpsession.h>

int main(void)
{
	// We can't use RTPSecureSession as an object since SRTP support
	// may not have been compiled in. The header should still exist
	// though
	jrtplib::RTPSession sess;
	return 0;
}
