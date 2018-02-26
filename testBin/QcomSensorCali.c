#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>  
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#define s16 int

#define G_SENSOR_FILE		"/sys/bus/i2c/drivers/mpu6880/4-0068/input/input2/GsensorData"
#define GY_SENSOR_FILE 		"/sys/bus/i2c/drivers/mpu6880/4-0068/input/input2/GYsensorData"
#define SENSOR_CALI_FILE	"/sys/bus/i2c/drivers/mpu6880/4-0068/input/input2/sensorCali"

struct axis_data {
	s16 x;
	s16 y;
	s16 z;
	s16 rx;
	s16 ry;
	s16 rz;
};
struct axis_data axis;


void readGSensorData(struct axis_data * data)
{
	int fd = open(G_SENSOR_FILE , O_RDONLY);
	if(fd < 0 ){
		printf("[readGSensorData] open %s failed!\n", G_SENSOR_FILE );
		return ;
	}
	char buf[40] = {0};
	read( fd, buf, sizeof(buf));
	sscanf(buf, "%d %d %d", &data->x , &data->y  ,&data->z );
	printf("[readGSensorData]%d %d %d\n" , data->x , data->y , data->z);
	close(fd);
}

void readGYSensorData(struct axis_data * data)
{
	int fd = open(GY_SENSOR_FILE , O_RDONLY);
	if(fd < 0 ){
		printf("[readGYSensorData] open %s failed!\n", GY_SENSOR_FILE );
		return ;
	}
	char buf[40] = {0};
	read( fd, buf, sizeof(buf));
	sscanf(buf, "%d %d %d", &data->rx , &data->ry  ,&data->rz );
	printf("[readGYSensorData]%d %d %d\n" , data->rx , data->ry , data->rz);
	close(fd);
}

void readSensorCali(struct axis_data * data)
{
	int fd = open(SENSOR_CALI_FILE , O_RDONLY);
	if(fd < 0 ){
		printf("[readSensorCali] open %s failed!\n", SENSOR_CALI_FILE );
		return ;
	}
	char buf[100] = {0};
	read( fd, buf, sizeof(buf));
	sscanf(buf, "x:%d y:%d z:%d\nrx:%d ry:%d rz:%d\n", &data->x , &data->y  ,&data->z ,
														&data->rx , &data->ry  ,&data->rz );
	printf("[readSensorCali]%d %d %d %d %d %d\n" , data->x , data->y , data->z,
													data->rx , data->ry , data->rz);
	close(fd);
}
void writeSensorCali(struct axis_data * data)
{
	int fd = open(SENSOR_CALI_FILE , O_WRONLY);
	if(fd < 0 ){
		printf("[readSensorCali] open %s failed!\n", SENSOR_CALI_FILE );
		return ;
	}
	char buf[100] = {0};
	sprintf(buf, "%d %d %d %d %d %d\n", data->x , data->y , data->z,
										data->rx , data->ry , data->rz);
	write( fd, buf, sizeof(buf));
	close(fd);
}
int main(int argc, char const *argv[])
{
	struct axis_data rawData[5];
	struct axis_data averageData = {0};
	struct axis_data caliData = {0};

	for(int i = 0; i < 5 ;i++){
		usleep(1000*500);
		readGSensorData(&rawData[i]);
		readGYSensorData(&rawData[i]);
		averageData.x += rawData[i].x;
		averageData.y += rawData[i].y;
		averageData.z += rawData[i].z;

		averageData.rx += rawData[i].rx;
		averageData.ry += rawData[i].ry;
		averageData.rz += rawData[i].rz;
	}

	averageData.x = averageData.x / 5;
	averageData.y = averageData.y / 5;
	averageData.z = averageData.z / 5;

	averageData.rx = averageData.rx / 5;
	averageData.ry = averageData.ry / 5;
	averageData.rz = averageData.rz / 5;

	printf("[average]%d %d %d %d %d %d\n" ,averageData.x , averageData.y , averageData.z,
										averageData.rx , averageData.ry , averageData.rz );
	int tmp1 , tmp2;
	for(int j = 0; j < 5 ; j++){
		tmp1 =   abs(averageData.x - rawData[j].x)
			  + abs(averageData.y - rawData[j].y)
			  + abs(averageData.z - rawData[j].z);
		printf("j[%d]%d " , j , tmp1);
		tmp2 =   abs(averageData.rx - rawData[j].rx)
			  + abs(averageData.ry - rawData[j].ry)
			  + abs(averageData.rz - rawData[j].rz);
		printf("%d\n" , tmp2);
		if(tmp1 > 150 || tmp2 > 20){
			printf("sensor calibrate failed\n");
			return 0;
		}
	}
	readSensorCali(&caliData);
	caliData.x += averageData.x;
	caliData.y += averageData.y - 9800;
	caliData.z += averageData.z;

	caliData.rx += averageData.rx;
	caliData.ry += averageData.ry;
	caliData.rz += averageData.rz;

	printf("[caliData]%d %d %d %d %d %d\n" ,caliData.x , caliData.y , caliData.z,
										caliData.rx , caliData.ry , caliData.rz );
	writeSensorCali(&caliData);
	printf("sensor calibrate success\n");
	return 0;
}