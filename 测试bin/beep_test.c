#include <stdio.h>
#include <sys/ioctl.h>
// #include <sys/type.h>
#include <fcntl.h>
//#include <cutils/log.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define FILENAME "/sys/bus/platform/devices/beep_pwm/beep"
static unsigned long count = 0 ;
int time = 1 ;
int main(int argc , char ** argv)
{
	if(argc == 2)
		time = atoi(argv[1]);
	int fd  = open(FILENAME, O_WRONLY );
	if(fd < 0 ){
		printf("[test] open %s failed!\n", FILENAME );
		return -1;
	}
	ssize_t ret = 0;
	while(1){
		printf("-----------------------------------------------------------\r\n");
		printf("\r\n");
		printf("           begin test!!!\r\n");
		printf("\r\n");
		printf("-----------------------------------------------------------\r\n");
		printf("[test] test TAKE_PHOTO voice!\r\n");
		ret =  write( fd, "1", strlen("1"));
		if(ret < 0)
		{
			printf("[test] write hellow world! to %s failed!\n", FILENAME );
			return -1;
		}
		usleep(time*1000000);

		printf("[test] test BEGIN_VEDIO voice!!\r\n");
		ret =  write( fd, "2", strlen("1"));
		if(ret < 0)
		{
			printf("[test] write hellow world! to %s failed!\n", FILENAME );
			return -1;
		}
		usleep(time*1000000);

		printf("[test] test CLOSE_VIDEO voice!!\r\n");
		ret =  write( fd, "3", strlen("1"));
		if(ret < 0)
		{
			printf("[test] write hellow world! to %s failed!\n", FILENAME );
			return -1;
		}
		usleep(time*1000000);

		printf("[test] test RECOVERY voice!!!\r\n");
		ret =  write( fd, "4", strlen("1"));
		if(ret < 0)
		{
			printf("[test] write hellow world! to %s failed!\n", FILENAME );
			return -1;
		}
		usleep(time*1000000);

		printf("[test] test WARNING voice!!!\r\n");
		ret =  write( fd, "5", strlen("1"));
		if(ret < 0)
		{
			printf("[test] write hellow world! to %s failed!\n", FILENAME );
			return -1;
		}
		usleep(5*1000000);
		
		//printf("[test] 测试 关闭 声!\r\n");
		//ret =  write( fd, "6", strlen("1"));
		//if(ret < 0)
		//{
		//	printf("[test] write hellow world! to %s failed!\n", FILENAME );
		//	return -1;
		//}
		//usleep(time*1000000);

		printf("[test] test CPU_TEMP_WARNING voice!\r\n");
		ret =  write( fd, "7", strlen("1"));
		if(ret < 0)
		{
			printf("[test] write hellow world! to %s failed!\n", FILENAME );
			return -1;
		}
		usleep(time*1000000);

		printf("[test] test AP_IRQ_HANDLER voice!!!\r\n");
		ret =  write( fd, "8", strlen("1"));
		if(ret < 0)
		{
			printf("[test] write hellow world! to %s failed!\n", FILENAME );
			return -1;
		}
		usleep(time*1000000);
		printf("-----------------------------------------------------------\r\n");
		printf("\r\n");
		printf("              已测%d 次！！！\r\n", ++count);
		printf("\r\n");
		printf("-----------------------------------------------------------\r\n");
		usleep(3*1000000);

	}
	close(fd);
}
