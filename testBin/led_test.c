#include <stdio.h>
#include <sys/ioctl.h>
// #include <sys/type.h>
#include <fcntl.h>
//#include <cutils/log.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define  RED_LED_FILE	"/sys/class/leds/red/brightness"
#define GREEN_LED_FILE	"/sys/class/leds/green/brightness"
#define BLUE_LED_FILE	"/sys/class/leds/blue/brightness"
#define WIFI_FILE	"/sys/class/leds/wifi/brightness"
#define WORKING_FILE	"/sys/class/leds/working/brightness"

#define RED_TRIGGER_FILE	"/sys/class/leds/red/trigger"
#define WORKING_TRIGGER_FILE	"/sys/class/leds/working/trigger"

static unsigned long count = 0 ;
static int time = 1 ;
ssize_t ret = 0 ;
int main(int argc , char ** argv)
{
	if(argc == 2)
		time = atoi(argv[1]);
	int fd_red  = open(RED_LED_FILE, O_WRONLY );
	if(fd_red < 0 ){
		printf("[test] open %s failed!\n", RED_LED_FILE );
		return -1;
	}
	int fd_green  = open(GREEN_LED_FILE, O_WRONLY );
	if(fd_green < 0 ){
		printf("[test] open %s failed!\n", GREEN_LED_FILE );
		return -1;
	}
	int fd_blue  = open(BLUE_LED_FILE, O_WRONLY );
	if(fd_blue < 0 ){
		printf("[test] open %s failed!\n", BLUE_LED_FILE );
		return -1;
	}
	int fd_wifi  = open(WIFI_FILE, O_WRONLY );
	if(fd_wifi < 0 ){
		printf("[test] open %s failed!\n", WIFI_FILE );
		return -1;
	}
	int fd_working  = open(WORKING_FILE, O_WRONLY );
	if(fd_working < 0 ){
		printf("[test] open %s failed!\n", WORKING_FILE );
		return -1;
	}
	int fd_red_trigger  = open(RED_TRIGGER_FILE, O_WRONLY );
	if(fd_red_trigger < 0 ){
		printf("[test] open %s failed!\n", RED_TRIGGER_FILE );
		return -1;
	}
	int fd_working_trigger  = open(WORKING_TRIGGER_FILE, O_WRONLY );
	if(fd_working_trigger < 0 ){
		printf("[test] open %s failed!\n", WORKING_TRIGGER_FILE );
		return -1;
	}
	while(1){
		printf("-----------------------------------------------------------\r\n");
		printf("\r\n");
		printf("              开始新一轮测试！！！\r\n");
		printf("\r\n");
		printf("-----------------------------------------------------------\r\n");
		printf("[test] 测试电源灯：红！\r\n");
		ret =  write( fd_red_trigger, "none", strlen("none"));
		ret =  write( fd_working_trigger, "none", strlen("none"));
		ret =  write( fd_red, "50", strlen("50"));
		ret =  write( fd_green, "0", strlen("0"));
		ret =  write( fd_blue, "0", strlen("0"));
		ret =  write( fd_working, "0", strlen("0"));
		ret =  write( fd_wifi, "0", strlen("0"));
		if(ret < 0)
		{
			printf("[test] write hellow world! failed!\n" );
			return -1;
		}
		usleep(time*1000000);

		printf("[test] 测试电源灯绿！\r\n");
		ret =  write( fd_red_trigger, "none", strlen("none"));
		ret =  write( fd_working_trigger, "none", strlen("none"));
		ret =  write( fd_red, "0", strlen("0"));
		ret =  write( fd_green, "50", strlen("50"));
		ret =  write( fd_blue, "0", strlen("0"));
		ret =  write( fd_working, "0", strlen("0"));
		ret =  write( fd_wifi, "0", strlen("0"));
		if(ret < 0)
		{
			printf("[test] write hellow world! failed!\n" );
			return -1;
		}
		usleep(time*1000000);
		printf("[test] 测试电源灯：白！\r\n");
		ret =  write( fd_red_trigger, "none", strlen("none"));
		ret =  write( fd_working_trigger, "none", strlen("none"));
		ret =  write( fd_red, "50", strlen("50"));
		ret =  write( fd_green, "50", strlen("50"));
		ret =  write( fd_blue, "50", strlen("50"));
		ret =  write( fd_working, "0", strlen("0"));
		ret =  write( fd_wifi, "0", strlen("0"));
		if(ret < 0)
		{
			printf("[test] write hellow world!  failed!\n" );
			return -1;
		}
		usleep(time*1000000);

		printf("[test] 测试电源灯：红色呼吸！\r\n");
		ret =  write( fd_red_trigger, "breath", strlen("breath"));
		ret =  write( fd_working_trigger, "none", strlen("none"));
		//ret =  write( fd_red, "50", strlen("50"));
		ret =  write( fd_green, "0", strlen("0"));
		ret =  write( fd_blue, "0", strlen("0"));
		ret =  write( fd_working, "0", strlen("0"));
		ret =  write( fd_wifi, "0", strlen("0"));
		if(ret < 0)
		{
			printf("[test] write hellow world!  failed!\n" );
			return -1;
		}
		usleep(2*1000000);

		printf("\r\n[test] 测试拍照灯：白！\r\n");
		ret =  write( fd_red_trigger, "none", strlen("none"));
		ret =  write( fd_working_trigger, "none", strlen("none"));
		ret =  write( fd_red, "0", strlen("0"));
		ret =  write( fd_green, "0", strlen("0"));
		ret =  write( fd_blue, "0", strlen("0"));
		ret =  write( fd_working, "0", strlen("0"));
		ret =  write( fd_wifi, "50", strlen("50"));
		if(ret < 0)
		{
			printf("[test] write hellow world!  failed!\n" );
			return -1;
		}
		usleep(time*1000000);

		printf("\r\n[test] 测试VIDEO灯：红！\r\n");
		ret =  write( fd_red_trigger, "none", strlen("none"));
		ret =  write( fd_working_trigger, "none", strlen("none"));
		ret =  write( fd_red, "0", strlen("0"));
		ret =  write( fd_green, "0", strlen("0"));
		ret =  write( fd_blue, "0", strlen("0"));
		ret =  write( fd_working, "50", strlen("50"));
		ret =  write( fd_wifi, "0", strlen("0"));
		if(ret < 0)
		{
			printf("[test] write hellow world! failed!\n");
			return -1;
		}
		usleep(time*1000000);

		printf("[test] 测试VIDEO灯：红色闪烁！\r\n");
		ret =  write( fd_red_trigger, "none", strlen("none"));
		ret =  write( fd_working_trigger, "timer", strlen("timer"));
		ret =  write( fd_red, "0", strlen("0"));
		ret =  write( fd_green, "0", strlen("0"));
		ret =  write( fd_blue, "0", strlen("0"));
		//ret =  write( fd_working, "0", strlen("0"));
		ret =  write( fd_wifi, "0", strlen("0"));
		if(ret < 0)
		{
			printf("[test] write hellow world!failed!\n");
			return -1;
		}
		usleep(2*1000000);
		ret =  write( fd_working_trigger, "none", strlen("none"));
		ret =  write( fd_working, "0", strlen("0"));
		
		usleep(2*1000000);
		printf("-----------------------------------------------------------\r\n");
		printf("\r\n");
		printf("              已测%d 次！！！\r\n", ++count);
		printf("\r\n");
		printf("-----------------------------------------------------------\r\n");
	}
	close(fd_red);
	close(fd_green);
	close(fd_blue);
	close(fd_working);
	close(fd_wifi);
	close(fd_red_trigger);
	close(fd_working_trigger);
}
