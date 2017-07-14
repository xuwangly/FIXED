/*
 *usage:a.out imagesize fps c
 */
#include <stdio.h>
#include   <sys/stat.h>
#include   <sys/types.h>
#include   <sys/socket.h>
#include   <stdio.h>
#include   <malloc.h>
#include   <netdb.h>
#include   <fcntl.h>
#include   <netinet/in.h>
#include   <arpa/inet.h>
#include <netinet/tcp.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define    RES_LENGTH  8192 //接受字符的最大长度
int   	sockfd=0;
int     connect_socket(const char * server,int serverPort);
int     send_msg(int sockfd,char * sendBuff ,int size);
void print_usage(char ** argv);
char *	send_buf = NULL;

int main(int argc, char ** argv)
{
	int imagesize = 0 ;
	int fps       = 0 ;
    if(argc != 4){
		print_usage(argv);
		return -1;
    }
	imagesize = atoi(argv[1])*1024 ;
    fps       = atoi(argv[2]) ;
	char c	  = argv[3][0];
	if(imagesize < 0 || fps < 0){
		print_usage(argv);
		return -1;
	}
	send_buf = (char *)malloc(imagesize);
	if(send_buf == NULL){
		printf("malloc failed\n");	
		return -1;
	}
    memset(send_buf ,c , imagesize);
    sockfd = connect_socket("127.0.0.1", 12580);
    if(sockfd < 0)
        return ;
    else
        printf("connect success! sockfd = %d\n" , sockfd);
	//设置no blocking
	/*
	int flags = fcntl(sockfd, F_GETFL, 0);
	int set_flag = fcntl(sockfd, F_SETFL, flags|O_NONBLOCK);
	printf("set flag return %d\n" , set_flag);
	 */
	setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (int[]){1}, sizeof(int));//使用KEEPALIVE
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (int[]){1}, sizeof(int));//禁用NAGLE算法
	int sendbuf  = 0 , recvbuf = 0  , len = sizeof(int);
	int ret1 = getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, &len);
	int ret2 = getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,  &recvbuf, &len);
	printf("%d  %d  send_buf:%d   recv_buf:%d        %s\n" , ret1 , ret2 ,sendbuf ,recvbuf  , strerror(errno) );
	//unsigned char cmd[10] = {0};
	send_buf[0]  = c ;
	send_buf[1]  = fps%256 ;
	send_buf[2]	= imagesize/1024%256;
    send(sockfd,send_buf,8192,0);
    
	int spf = 1000000/fps;
	struct timeval  current_time;
    while(1){
		//sleep(10);
		//int package = imagesize / 1024*8
        int n = send_msg(sockfd , send_buf , imagesize);
		gettimeofday( &current_time, NULL );
		printf("send_msg return %d   error:%s    time:%lu\n" , n , strerror(errno)  , 1000000 *  current_time.tv_sec + current_time.tv_usec );
        //usleep(spf);
    }
	free(send_buf);
    close(sockfd);
}

int    connect_socket(const char * server,int serverPort){
    int    sockfd=0;
    struct    sockaddr_in    addr;
    struct    hostent        * phost;

    if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0){
        herror("Init socket error!");
        return -1;
    }
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(serverPort);
    addr.sin_addr.s_addr = inet_addr(server);//按IP初始化

    if(addr.sin_addr.s_addr == INADDR_NONE){//如果输入的是域名
        phost = (struct hostent*)gethostbyname(server);
        if(phost==NULL){
            herror("Init socket s_addr error!");
            return -1;
        }
        addr.sin_addr.s_addr =((struct in_addr*)phost->h_addr)->s_addr;
    }
    if(connect(sockfd,(struct sockaddr*)&addr, sizeof(addr))<0)
    {
        perror("Connect server fail!");
        return -1; //0表示成功，-1表示失败
    }
    else
        return sockfd;
}

int send_msg(int sockfd,char * sendBuff , int size)
{
    int sendSize=0;
    if((sendSize=send(sockfd,sendBuff,size,0))<=0){
        herror("Send msg error!");
        return -1;
    }else
        return sendSize;
}
void print_usage(char ** argv)
{
	printf("usage:%s imagesize(kb) fps c\n" , argv[0]);
}
