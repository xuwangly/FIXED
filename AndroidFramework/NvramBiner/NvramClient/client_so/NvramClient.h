#ifndef ANDROID_NVRAM_CLIENT_H  
#define ANDROID_NVRAM_CLIENT_H  
#include <binder/Parcel.h>  
  
namespace android  
{  
    class NvramClient  
    {  
    public:  
        int readNv(String8 name, void *buf, size_t count);
        int writeNv(String8 name, void *buf, size_t count);

    private:  
        static void getNvramService();
        int readInner(String8 name ,int i , size_t count , void *buf );  
        int writeInner(String8 name ,int i , size_t count , void *buf );

        const unsigned long TRANSFER_LIMIT = 1024*512;
        struct  Map
        {
            char    name[20];
            int     Lid;
            int     size;
        };
        struct Map map[3] = {
            [0] = {
                .name = "H1",
                .Lid  = 71,
                .size = sizeof(float)*20,
            },
            [1] = {
                .name = "offSetSide",
                .Lid  = 72,
                .size = sizeof(int)*200,
            },
            [2] = {
                .name = "H",
                .Lid  = 73,
                .size = sizeof(float)*640*480*2,
            }
        };
        enum
        {
            SUCCESS                 =  0,
            ERROR_SERVICE_NOT_FOUND = -1,
            ERROR_NO_FILE           = -2,
            ERROR_PARAMETER         = -3,
            ERROR_OPENLIDFILE       = -4,
        };
    };  
}    
#endif