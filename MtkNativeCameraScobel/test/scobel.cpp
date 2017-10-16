#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgproc.hpp";
#include "opencv2/imgcodecs.hpp";
#include "opencv2/core.hpp"
#include <stdio.h>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace cv;
using namespace std;

bool readBufFromFile(char const*const fname, uint8_t *const buf, uint32_t const size)
{
    int nw, cnt = 0;
    uint32_t written = 0;

    printf("opening file [%s]\n", fname);
    int fd = open(fname, O_RDWR);
    if (fd < 0) {
        printf("failed to create file [%s]: %s\n", fname, ::strerror(errno));
        return false;
    }
    printf("opening file [%d]\n", fd);
    printf("read %d bytes to file [%s]\n", size, fname);
    while (written < size) {
        nw = read(fd,
                     buf + written,
                     size - written);
        if (nw < 0) {
            printf("failed to read from file [%s]: %s\n", fname, ::strerror(errno));
            break;
        }
        written += nw;
        cnt++;
    }
    printf("done read %d bytes from file [%s] in %d passes\n", size, fname, cnt);
    close(fd);
    return true;
}
int main(int argc, char const *argv[])
{
	unsigned char * buf = (unsigned char *)malloc(307200);
	readBufFromFile("/sdcard/LOLOLOLOLOLyuvImgimageSobel.yuv" , buf , 307200);

	Mat imageSobel;
	imageSobel.create(640, 480, CV_8UC1);
	memcpy(imageSobel.data, buf, 307200*sizeof(unsigned char));

	double meanValue = mean(imageSobel)[0];

	printf("%f\n" , meanValue);
	return 0;
}