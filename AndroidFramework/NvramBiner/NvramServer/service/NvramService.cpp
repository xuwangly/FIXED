#include <binder/IServiceManager.h>  
#include <binder/IPCThreadState.h>  
#include "NvramService.h"    
#include "libnvram.h"
#include "libfile_op.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "Custom_NvRam_LID.h"
#include <cutils/log.h>
#include <stdlib.h>
#include <cutils/properties.h>
namespace android  
{  
    //static struct sigaction oldact;  
    static pthread_key_t sigbuskey;  
      
    int NvramService::Instance()  
    {  
        DebugPrint("NvramService Instantiate\n");  
        int ret = defaultServiceManager()->addService(  
                String16(NvramService::getServiceName()), new NvramService() );  
        DebugPrint(" NvramService ret = %d\n", ret);  
        return ret;  
    }  
  
    NvramService::NvramService()  
    {  
        DebugPrint(" NvramService create\n");  
        //m_NextConnId = 1;  
        //pthread_key_create(&sigbuskey,NULL);  
    }  
  
    NvramService::~NvramService()  
    {  
        //pthread_key_delete(sigbuskey);  
        DebugPrint(" NvramService destory\n");  
    }
    int NvramService::readFromNv(struct  Map  *mMap , int offset , int size , Parcel* reply)
    {
        int rec_size=0 , rec_num = 0 , result = 0;
        //open file
        F_ID f_id = NVM_GetFileDesc(mMap->Lid, &rec_size, &rec_num,0);
        //set offset
        lseek(f_id.iFileDesc, offset, SEEK_SET);
        uint8_t* nvdata = (uint8_t*)reply->writeInplace(size); 
        if(nvdata){
            result = read(f_id.iFileDesc , nvdata , size);
            DebugPrint("readFromNv:read:%d" , result);
        }
        NVM_CloseFileDesc(f_id);
        return result;
    }
    int NvramService::writeToNv(struct  Map  *mMap , int offset , int size , const Parcel& data)
    {
        int rec_size=0 , rec_num = 0 , result = 0;
        //open file
        F_ID f_id = NVM_GetFileDesc(mMap->Lid, &rec_size, &rec_num,0);
        //set offset
        lseek(f_id.iFileDesc, offset, SEEK_SET);

        uint8_t * nvdata  =  const_cast<uint8_t *>(data.data());
        nvdata +=  data.dataPosition();
        if(nvdata){
            result = write(f_id.iFileDesc,nvdata, size);
            DebugPrint("writeToNv:write:%d" , result);
        }
        NVM_CloseFileDesc(f_id);
        return result;
    }
    status_t NvramService::onTransact(uint32_t code,   
                                 const Parcel& data,   
                                 Parcel* reply,  
                                 uint32_t flags)  
    {
        String8 name              =   data.readString8();
        int     readWriteSize     =   data.readInt32();
        int     offset            =   data.readInt32();
        struct  Map  *mMap = NULL;
        DebugPrint("[Service:onTransact] name:%s readWriteSize:%d offset:%d" ,name.string(),readWriteSize , offset );
        for(int i = 0 ; i < sizeof(map) ; i++){
            if(!strcmp(name.string(), map[i].name)){
                mMap = &map[i];
            }
        }
        if(mMap == NULL){
            DebugPrint("[Service:onTransact] cannot find %s file!" ,name.string() );
            return NO_ERROR;
        }
        switch(code)  
        {  
        //read from Nvram
        case 0:   
            {  
                //reply->writeInt32(readFromNv(mMap , offset , readWriteSize , reply));
                readFromNv(mMap , offset , readWriteSize , reply);
                return NO_ERROR;
            } break; 
        //write to Nvram 
        case 1:   
            {  
                //reply->writeInt32(writeToNv(mMap , offset , readWriteSize , data));
                writeToNv(mMap , offset , readWriteSize , data);
                return NO_ERROR;
            } break; 
        case 2:
            {
                // int ret = FileOp_BackupToBinRegion_All();
                int ret = property_set("persist.nvram.backup", "1");
                DebugPrint("[LOL][Service:onTransact]property_set return %d" , ret);
                return NO_ERROR;
            } break; 
        default:  
            return BBinder::onTransact(code, data, reply, flags);  
        }  
    }  
}
