#include <stdio.h>  
//#include "../svcclient/JACK_Client.h"  
#include "../client_so/NvramClient.h"
#include "../log.h"  
#include <utils/String8.h>  
  
using namespace android;  
  
float data[640*480*2] ;
int offSetSide[200];
float H1[20];
int main(int argc, char** argv)  
{  
    NvramClient nvramClient;
    for (int i = 0; i < 1; ++i)
    {
        nvramClient.read(String8("H") ,data , 640*480*2*4 );
        for(int i = 0; i < 640*480*2 ; i++){
            if(!(i%10000))
                printf("%4f  ",data[i] );
            if(!(i%100000))
                printf("\n");
            data[i]++;
        }
        nvramClient.write(String8("H") ,data , 640*480*2*4 );
    }
    return 0;  
}