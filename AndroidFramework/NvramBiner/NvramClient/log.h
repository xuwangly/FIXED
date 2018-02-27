#define __DEBUG__   
#define TAG "chyj"   
#ifdef __DEBUG__ //改进方法  
#define DebugPrint(fmt,args...) ALOGD("-"  fmt  "\n",  ##args);  
#else  
#define DebugPrint(fmt,args...)   
#endif