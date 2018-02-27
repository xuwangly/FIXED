#include <binder/IServiceManager.h>  
#include <binder/IPCThreadState.h>  
#include "NvramClient.h"
#include "libnvram.h"
#include "libfile_op.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "Custom_NvRam_LID.h"
#include "../log.h"
#include<android/log.h>   
namespace android  
{  
    sp<IBinder> binder;  
    int NvramClient::readInner(String8 name ,int i , size_t count , void *buf )
    {
        Parcel data, reply;
        data.writeString8(name);
        data.writeInt32((count - TRANSFER_LIMIT*i < TRANSFER_LIMIT)?(count - TRANSFER_LIMIT*i):TRANSFER_LIMIT );
        //offset
        data.writeInt32(TRANSFER_LIMIT*i);
        binder->transact(0, data, &reply);
        DebugPrint("[Client:read] i:%d dataAvail:%d" , i , reply.dataAvail());//dataAvail()
        memcpy(buf + TRANSFER_LIMIT*i , reply.data(), reply.dataAvail());
        return 0;
    }
    int NvramClient::writeInner(String8 name ,int i , size_t count , void *buf )
    {
        Parcel data, reply;
        data.writeString8(name);
        //writesize
        data.writeInt32((count - TRANSFER_LIMIT*i < TRANSFER_LIMIT)?(count - TRANSFER_LIMIT*i):TRANSFER_LIMIT );
        //offset
        data.writeInt32(TRANSFER_LIMIT*i);
        data.write(buf + TRANSFER_LIMIT*i, (count - TRANSFER_LIMIT*i < TRANSFER_LIMIT)?(count - TRANSFER_LIMIT*i):TRANSFER_LIMIT);
        binder->transact(1, data, &reply);
        return 0;
    }
    #if 0
    int NvramClient::readNv(String8 name, void *buf, size_t count)  
    {  
        getNvramService();
        if(binder == 0)  
        {  
            DebugPrint(" JACK_Service not published, waiting...");  
            return ERROR_SERVICE_NOT_FOUND;  
        }
        struct  Map  *mMap = NULL;
        for(int i = 0 ; i < sizeof(map)/sizeof(map[0]) ; i++){
            if(!strcmp(name.string(), map[i].name)){
                mMap = &map[i];
            }
        }
        if(mMap == NULL)
            return ERROR_NO_FILE;
        if(buf == NULL || count <= 0)
            return ERROR_PARAMETER;

        int readsize = (count < mMap->size) ? count : mMap->size;
        int loop = readsize / TRANSFER_LIMIT + ((readsize % TRANSFER_LIMIT) ? 1 : 0);
        DebugPrint("[Client:read]  readsize:%d  loop:%d" , readsize , loop);

        int r[loop];
        for(int i = 0 ; i < loop ; i++){
            readInner(name ,i , count , buf );
        }

        return 0;  
    }  
    int NvramClient::writeNv(String8 name, void *buf, size_t count) 
    {  
        getNvramService();  
        if(binder == 0)  
        {  
            DebugPrint(" JACK_Service not published, waiting...");  
            return ERROR_SERVICE_NOT_FOUND;  
        }
        struct  Map  *mMap = NULL;
        for(int i = 0 ; i < sizeof(map)/sizeof(map[0]) ; i++){
            if(!strcmp(name.string(), map[i].name)){
                mMap = &map[i];
            }
        }
        if(mMap == NULL)
            return ERROR_NO_FILE;
        if(buf == NULL || count <= 0)
            return ERROR_PARAMETER;

        int writesize = (count < mMap->size) ? count : mMap->size;
        int loop = writesize / TRANSFER_LIMIT + ((writesize % TRANSFER_LIMIT) ? 1 : 0);
        DebugPrint("[Client:write]  writesize:%d  loop:%d" , writesize , loop);

        for(int i = 0 ; i < loop ; i++){
            writeInner(name ,i , count , buf );
        }
        //backup to nvram
        Parcel data, reply;
        binder->transact(2, data, &reply);
        return 0;
    }
    #else
    int NvramClient::readNv(String8 name, void *buf, size_t count)
    {
        struct  Map  *mMap = NULL;
        for(int i = 0 ; i < sizeof(map)/sizeof(map[0]) ; i++){
            DebugPrint("%s\n%s\n", name.string() , map[i].name);
            if(!strcmp(name.string(), map[i].name)){
                mMap = &map[i];
            }
        }
        if(mMap == NULL)
            return ERROR_NO_FILE;
        if(buf == NULL || count <= 0)
            return ERROR_PARAMETER;

        int rec_size=0 , rec_num = 0 , result = 0;
        //open file
        F_ID f_id = NVM_GetFileDesc(mMap->Lid, &rec_size, &rec_num,0);
        if(f_id.iFileDesc < 0){
            DebugPrint("NVM_GetFileDesc fail! Lid:%d\n" ,mMap->Lid );
            return ERROR_OPENLIDFILE;
        }
        DebugPrint("f_id.iFileDesc:%d  size:%d\n", f_id.iFileDesc , (count <rec_size *rec_num)?count:(rec_size *rec_num));
        result = read(f_id.iFileDesc , buf , (count <rec_size *rec_num)?count:(rec_size *rec_num));
        DebugPrint("read %d from %s\n" , result , mMap->name);
        NVM_CloseFileDesc(f_id);
        return result;
    }  
    int NvramClient::writeNv(String8 name, void *buf, size_t count)
    {
        struct  Map  *mMap = NULL;
        for(int i = 0 ; i < sizeof(map)/sizeof(map[0]) ; i++){
            if(!strcmp(name.string(), map[i].name)){
                mMap = &map[i];
            }
        }
        if(mMap == NULL)
            return ERROR_NO_FILE;
        if(buf == NULL || count <= 0)
            return ERROR_PARAMETER;

        int rec_size=0 , rec_num = 0 , result = 0;
        //open file
        F_ID f_id = NVM_GetFileDesc(mMap->Lid, &rec_size, &rec_num,0);
        if(f_id.iFileDesc < 0){
            DebugPrint("NVM_GetFileDesc fail! Lid:%d\n" ,mMap->Lid );
            return ERROR_OPENLIDFILE;
        }
        result = write(f_id.iFileDesc , buf , (count <rec_size *rec_num)?count:(rec_size *rec_num));
        DebugPrint("write %d to %s\n" , result , mMap->name);
        NVM_CloseFileDesc(f_id);

        //backup to nvram
        getNvramService();
        if(binder == 0)
        {
            DebugPrint(" JACK_Service not published, waiting...");
            return ERROR_SERVICE_NOT_FOUND;
        }
        Parcel data, reply;
        binder->transact(2, data, &reply);
        return result;
    }
    #endif
    void NvramClient::getNvramService()  
    {  
        sp<IServiceManager> sm = defaultServiceManager();  
        binder = sm->getService(String16("nvram.service"));  
        DebugPrint(" client - etService: %p\n", sm.get());  
        if(binder == 0)  
        {  
            DebugPrint(" nvram.service not published, waiting...");  
            return;  
        }  
    }  
  
}