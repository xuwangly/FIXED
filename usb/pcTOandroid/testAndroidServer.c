//
// Created by wangxu on 17-7-5.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <netinet/tcp.h>

pthread_t connect_1_tid;
pthread_t connect_2_tid;
pthread_t connect_3_tid;
pthread_t connect_4_tid;

void *thread1(void *arg);
//void *thread2(void *arg);

struct arg{  
    int		connfd;
    int 	fps;
	int		imagesize;//kb
	int 	c;  
};  
#define MAXLINE 4096

int main(int argc, char** argv)
{
    int    listenfd, connfd;
    struct sockaddr_in     servaddr;
    char    buff[8296];
    int     n;
    int connect_count = 0;
    int recv_cmd = -1;

    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(10086);

    if( bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }

    if( listen(listenfd, 1) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }

    printf("======waiting for client's request======\n");
    while(1){
        if( (connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            continue;
        }
		
		//设置opt选项
        struct timeval timeout={3,0};//3s
		int ret = 0;
        //ret +=	setsockopt(sock_fd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
		ret +=	setsockopt(listenfd, SOL_SOCKET, SO_KEEPALIVE, (int[]){1}, sizeof(int));//使用KEEPALIVE
		ret += 	setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, (int[]){1}, sizeof(int));//禁用NAGLE算法
		ret += 	setsockopt(listenfd, IPPROTO_TCP, TCP_QUICKACK, (int[]){1}, sizeof(int));
        ret += 	setsockopt(connfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));
        if(ret != 0){
            printf("setsockopt error\n");
        }
		
	struct arg myarg;
        //如果ret==0 则为成功,-1为失败,这时可以查看errno来判断失败原因
        int recvd=recv(connfd,buff,8296,0);
        if(recvd==-1&&errno==EAGAIN){
            printf("timeout\n");
            close(connfd);
            continue;
        }else{
            myarg.c			= buff[0];
	    	myarg.connfd 	= connfd;
			myarg.fps 		= buff[1];
			myarg.imagesize	= buff[2]; 
        }
		struct timeval timeout1={0,0};//3s
        //int ret=setsockopt(sock_fd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
        ret=setsockopt(connfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout1,sizeof(timeout1));
        if(ret != 0){
            printf("setsockopt error\n");
        }
		pthread_t  *connect_tid = malloc(sizeof(pthread_t));
		int err = pthread_create(connect_tid, NULL, thread1, &myarg); //创建线程
        if(err != 0){
            printf("create thread error: %s/n",strerror(err));
            return 1;
        }
    }
    //pthread_join(connect_1_tid,NULL);
    //pthread_join(connect_2_tid,NULL);
    close(listenfd);
}

void *thread1(void *arg)
{
	struct timeval old_time, current_time;
	printf("thread1 connect\n");
	struct arg *myarg = (struct arg *)arg;
    int 	connfd 		= myarg->connfd ;
	int 	fps			= myarg->fps ;
	int 	c			= myarg->c ;
	int 	imagesize	= myarg->imagesize * 1024 ;
    char	*buff;
	
	/*
    unsigned long ul=1;  
    int r=ioctlsocket(servSocket,FIONBIO,&ul);  
    if(r==SOCKET_ERROR)  
    {  
        return false;  
    }  */
	int sendbuf  = 0 , recvbuf = 0  , len = sizeof(int);
	int ret1 = getsockopt(connfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, &len);
	int ret2 = getsockopt(connfd, SOL_SOCKET, SO_RCVBUF,  &recvbuf, &len);
	printf("%d  %d  send_buf:%d   recv_buf:%d        %s\n" , ret1 , ret2 ,sendbuf ,recvbuf  , strerror(errno) );
	
	
	gettimeofday( &old_time, NULL );
	gettimeofday( &current_time, NULL );
	buff = (char *)malloc(imagesize);
    int count = 0 ;
	unsigned long recv_count = 0 ;
    while(1){
		gettimeofday( &current_time, NULL );
		if((1000000 * ( current_time.tv_sec - old_time.tv_sec ) + current_time.tv_usec - old_time.tv_usec) > 1000000 ){
			printf("pid:%ld recv %lu byte msg from client  --count:%d--  speed:%d\n", (long int)syscall(SYS_gettid) , recv_count ,count ,recv_count/1024);
			//printf("pid:%ld recv speed:%d\n" , (long int)syscall(SYS_gettid) , recv_count/1024);

			recv_count = 0;
			old_time = current_time;
		}
        count++;
        int n = recv(connfd, buff, imagesize, 0);
		//printf("pid:%ld recv:%d    time:%lu \n",(long int)syscall(SYS_gettid) ,n , 1000000 *  current_time.tv_sec + current_time.tv_usec);
		//接受后设置TCP_QUICKACK
		//int setopt = setsockopt(connfd, IPPROTO_TCP, TCP_QUICKACK, (int[]){1}, sizeof(int)); 
		//printf("pid:%ld recv error: recv return %d\n  errno:%s\n" , (long int)syscall(SYS_gettid) ,setopt , strerror(errno));
		if(n <= 0){
					printf("pid:%ld recv error: recv return %d\n  errno:%s\n" , (long int)syscall(SYS_gettid) ,n , strerror(errno));
					int try_send = send(connfd,"hellow!",strlen("hellow!"),0);
					if(try_send < 0 || errno != 0){
								printf("pid:%ld try send failed return  %d\n  errno:%s  close socket\n" , (long int)syscall(SYS_gettid) ,try_send , strerror(errno));
								break;
					}
		}
		char * tmp = malloc(imagesize);
		memcpy(tmp , buff , imagesize);
		free(tmp);
		recv_count += n;
        //buff[n] = '\0';
		/*if(!(count%100))
			printf("pid:%ld recv %d byte msg from client: %s\n", (long int)syscall(SYS_gettid) , n ,buff);
		*/
        /*if(count%100)
            printf("pid:%ld recv %d byte msg from client\n", (long int)syscall(224) , n );
        else
            printf("pid:%ld recv %d byte msg from client: %s\n", (long int)syscall(224) , n ,buff);
		*/
    }
	free(buff);
    close(connfd);
	pthread_exit(0);
}
/*
void *thread2(void *arg)
{
	printf("thread2 connect\n");
	struct arg *myarg = (struct arg *)arg;
    int 	connfd 		= myarg->connfd ;
	int 	fps			= myarg->fps ;
	int 	c			= myarg->c ;
	int 	imagesize	= myarg->imagesize * 1024 ;
    char	*buff;

	buff = (char *)malloc(imagesize);
    int count = 0 ;
    while(1){
        count++;
        int n = recv(connfd, buff, imagesize, 0);
        buff[n] = '\0';
        if(count%100)
            printf("thread2 recv %d byte msg from client\n", n );
        else
            printf("thread2 recv %d byte msg from client: %s\n", n ,buff);
    }
	free(buff);
    close(connfd);
}
*/
