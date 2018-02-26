#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>  
#include <string.h>
#include <stdio.h>
#include <unistd.h>


static unsigned char I2C_read_reg(int ,unsigned  , unsigned );
static unsigned char I2C_write_reg(int fd ,unsigned slave , unsigned chip_addr , unsigned val);

#define SLAVE_ADDR 	(0xb6 >> 1)
#define LENGTH		6	

void testPac(int fd)
{
	
	for(int i = 0; i < 8 ; i++)
		I2C_read_reg(fd ,33  , i );
	//I2C_write_reg (fd ,33 , 6 , 0xf0);
	//I2C_write_reg (fd ,33 , 7 , 0xf0);
	//int ret = I2C_read_reg(fd ,33  , 7 );
	//printf("0x%x\n" , ret);
}
int main(int argc , char * argv[])
{	
	unsigned long  funcs = 0;
	int smbus_fp;
	if(argc == 2){
		char buf[50] = {0};
		sprintf( buf,"/dev/i2c-%s", argv[1] );
		if((smbus_fp = open(buf , O_RDWR)) < 0 ){
			printf("open  %s error\n" , buf);
			return -1;	
		}
	}else {
		if((smbus_fp = open("/dev/i2c-1" , O_RDWR)) < 0 ){
			printf("open  /dev/i2c-1 error\n");
			return -1;	
		}
	}
	/*if(ioctl(smbus_fp , I2C_SLAVE ,SLAVE_ADDR) < 0 ){
		printf("errno %d  %s\n",errno ,strerror(errno)); 
		return -1;
	}*/
	//set RETRIES 
	ioctl(smbus_fp, I2C_RETRIES,2);
	//get functions
	if(ioctl(smbus_fp,I2C_FUNCS,&funcs) < 0 ){
		printf("errno %d  %s\n",errno ,strerror(errno)); 
		return -1;
	}
	char s[20] = {0};
	//itoa(funcs, s, 2);
	printf("funcs --> 0x%x\n", funcs);
	if(!(funcs & 1)){
		printf("donot support I2C_FUNC_I2C\n");
		return -1;
	}
	//testPac(smbus_fp);
	
	while(1){
		I2C_read_reg(smbus_fp, 0x2c, 0);
		//msleep(50);	
	}
	/*for(unsigned i = 0 ; i < 0xff ; i ++ ){
		for(unsigned j = 0; j < 1 ; j++){
			I2C_read_reg(smbus_fp, i, j);
			usleep(1000);
		}	
	}*/
	close(smbus_fp);
}
static unsigned char I2C_write_reg(int fd ,unsigned slave , unsigned chip_addr , unsigned val)
{
    int ret;
    unsigned char buf[3];
	struct i2c_rdwr_ioctl_data ioctl_data;

    buf[0] = chip_addr;
    buf[1] = val;

    struct i2c_msg msg[] =
    {
        {
            .addr	= slave,
            .flags	= 0,
            .len	= 2,
            .buf	= buf,
        },
    };
	ioctl_data.nmsgs = 1;
	ioctl_data.msgs = &msg[0];
    ret = ret = ioctl(fd, I2C_RDWR, &ioctl_data);
	if (ret < 0) {
  		printf("write I2C_RDWR failed slave:%d %d %s.\n", slave , errno ,strerror(errno));
  		return (-1);
 	}
	printf("write to slave:%u register:%u return:%u\n" , slave,chip_addr , ret);
    return ret;
}

static unsigned char I2C_read_reg(int fd ,unsigned slave , unsigned chip_addr)
{
    int ret;
    unsigned char buf[2] = {0};
    buf[0] = chip_addr;
	struct i2c_rdwr_ioctl_data ioctl_data;
    struct i2c_msg msgs[] =
    {
        {
            .addr	= slave,//slave
            .flags	= 0,
            .len	= 1,
            .buf	= &buf[0],
        },
        {
            .addr	= slave,
            .flags	= 1,
            .len	= 1,
            .buf	= &buf[1],
        },
    };
	ioctl_data.nmsgs = 2;
 	ioctl_data.msgs = &msgs[0];

	ret = ioctl(fd, I2C_RDWR, &ioctl_data);
 	if (ret < 0) {
  		printf("read I2C_RDWR failed slave:%d %d %s.\n", slave , errno ,strerror(errno));
  		return (-1);
 	}
	printf("read from slave:%u register:%u return:%u\n" , slave,chip_addr , buf[1]);
    return buf[1];
}
