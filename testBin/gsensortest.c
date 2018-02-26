#include <stdio.h>
#include <sys/ioctl.h>
// #include <sys/type.h>
#include <fcntl.h>
//#include <cutils/log.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>


struct hwmsen_convert {
	float sign[4];
	unsigned char map[4];
};
struct hwmsen_convert map[] = {
	{ { 1, 1, 1}, {0, 1, 2} },
	{ {-1, 1, 1}, {1, 0, 2} },
	{ {-1, -1, 1}, {0, 1, 2} },
	{ { 1, -1, 1}, {1, 0, 2} },

	{ {-1, 1, -1}, {0, 1, 2} },
	{ { 1, 1, -1}, {1, 0, 2} },
	{ { 1, -1, -1}, {0, 1, 2} },
	{ {-1, -1, -1}, {1, 0, 2} },

};

#define FILENAME "/sys/bus/platform/drivers/gsensor/sensordata"
static unsigned long count = 0 ;

int main(int argc , char ** argv)
{
	int fd  = open(FILENAME, O_RDONLY );
	if(fd < 0 ){
		printf("[test] open %s failed!\n", FILENAME );
		return -1;
	}
	
	char buf[40] = {0};
	ssize_t count = read( fd, buf, sizeof(buf));
	
	int raw[3];
	int ret = sscanf(buf, "%x %x %x", &raw[0] , &raw[1] , &raw[2]);
	
	float data[3];
	
	for(int i = 0 ; i < 3 ; i++)
		data[i] = raw[i]/100.0;
	
	printf("%x %x %x\n" , raw[0] , raw[1] , raw[2]);
	printf("%f %f %f\n\n\n" , data[0] , data[1] , data[2]);

	for(int i = 0 ; i < 8 ; i++)
		printf("%d:	%16lf %16lf %16lf\n" ,i ,map[i].sign[0]*1.0*data[map[i].map[0]] , map[i].sign[1]*data[map[i].map[1]] , map[i].sign[2]*data[map[i].map[2]]);
	close(fd);
}
