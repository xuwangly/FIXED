#include <stdio.h>  
#include <sys/types.h>  
#include <unistd.h>  
#include <grp.h>  
#include <binder/IPCThreadState.h>  
#include <binder/ProcessState.h>  
#include <binder/IServiceManager.h>  
#include <utils/Log.h>  
#include "../service/NvramService.h"  
  
using namespace android;  
  
  
int main(int arg, char** argv)  
{   
    DebugPrint(" server - ain() begin\n");  
    sp<ProcessState> proc(ProcessState::self());  
    sp<IServiceManager> sm = defaultServiceManager();   
    DebugPrint(" server - erviceManager: %p\n", sm.get());  
/*    int ret = JACK_Service::Instance();  
    DebugPrint(" server - JACK_Service::Instance return %d\n", ret); 
    ret = AutoFocusService::Instance();  
    DebugPrint(" server - AutoFocusService::Instance return %d\n", ret);*/
    int ret = NvramService::Instance();  
    DebugPrint(" server - NvramService::Instance return %d\n", ret);  
    ProcessState::self()->startThreadPool();  
    IPCThreadState::self()->joinThreadPool();  
  
    return 0;  
}