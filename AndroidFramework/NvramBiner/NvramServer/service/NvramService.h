#ifndef ANDROID_NVRAM_SERVICE_H  
#define ANDROID_NVRAM_SERVICE_H  
  
#include <utils/RefBase.h>  
#include <binder/IInterface.h>  
#include <binder/Parcel.h>
#include <cutils/log.h> 
#define DebugPrint(fmt,args...) ALOGE("[NvramService]"  fmt ,  ##args); 
  
namespace android  
{  
    class NvramService : public BBinder
    {  
    private:  
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

        };
        int writeToNv(struct  Map  *mMap , int offset , int size , const Parcel& data);
        int readFromNv(struct  Map  *mMap , int offset , int size , Parcel* reply);
  
    public:  
        static int Instance();  
        NvramService();  
        virtual ~NvramService();  
        virtual status_t onTransact(uint32_t, const Parcel&, Parcel*, uint32_t); 
        //add wangxu
        static char const* getServiceName() { return "nvram.service"; } 
    };  
}  
  
#endif