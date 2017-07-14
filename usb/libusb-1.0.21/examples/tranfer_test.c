/*2014.09.17 
g++ generic_mouse.c -o generic_mouse `PKG_CONFIG_PATH=/home/lei_wang/usr_lib/lib/pkgconfig/ pkg-config --cflags --libs libusb-1.0` 
sudo ./generic_mouse 
 
the code below is to read mouse data,it works fine. 
run the program under sudo, and move the mouse, can see the data print out. 
*/  
#include <iostream>  
#include <stdio.h>  
#include <libusb.h>  
using namespace std;  
  
int main() {  
    libusb_device **devs;          //pointer to pointer of device, used to retrieve a list of devices  
    libusb_context *ctx = NULL;    //a libusb session  
    int r;                         //for return values  
    ssize_t cnt;                   //holding number of devices in list  
    r = libusb_init(&ctx);         //initialize a library session  
    if(r < 0) {  
        printf("Init Error %d\n" , r); //there was an error  
        return 1;  
    }  
    libusb_set_debug(ctx, 3); //set verbosity level to 3, as suggested in the documentation  
    cnt = libusb_get_device_list(ctx, &devs); //get the list of devices  
    if(cnt < 0) {  
        cout<<"Get Device Error"<<endl;   //there was an error  
    }  
  
    libusb_device_handle *dev_handle;         //a device handle  
    dev_handle = libusb_open_device_with_vid_pid(ctx, 0x05c6, 0x9999); //open mouse  
    if(dev_handle == NULL) {  
            printf("Cannot open device\n");  
            libusb_free_device_list(devs, 1); //free the list, unref the devices in it  
            libusb_exit(ctx);                 //close the session  
            return 0;  
    } else {  
        printf("Device Opened\n");  
        libusb_free_device_list(devs, 1);                     //free the list, unref the devices in it  
        if(libusb_kernel_driver_active(dev_handle, 3) == 1) { //find out if kernel driver is attached  
            printf("Kernel Driver Active\n");  
            if(libusb_detach_kernel_driver(dev_handle, 3) == 0) //detach it  
                printf("Kernel Driver Detached!\n");  
        }  
        r = libusb_claim_interface(dev_handle, 3);            //claim interface 0 (the first) of device (mine had jsut 1)  
        if(r < 0) {  
            printf("Cannot Claim Interface\n");  
            return 1;  
        }  
        printf("Claimed Interface\n");  
        int size;  
        unsigned char datain[1024]="\0";  
        for(int i=0;i<300;i++)  
        {  
            int rr = libusb_interrupt_transfer(dev_handle,  
                    0x85,  
                    datain,  
                    0x0004,  
                    &size,  
                    1000);  
            printf("libusb_interrupt_transfer rr: %d \n" , rr);  
            printf("size: %d\n" ,size);  
            printf("data: ");  
            for(int j=0; j<size; j++)  
                  printf("%02x ", (unsigned char)(datain[j]));  
            printf("\n");  
        }  
  
        r = libusb_release_interface(dev_handle, 3); //release the claimed interface  
        if(r!=0) {  
            printf("Cannot Release Interface\n");
            return 1;  
        }  
                printf("Released Interface\n");  
  
                libusb_attach_kernel_driver(dev_handle, 3);  
                libusb_close(dev_handle);  
                libusb_exit(ctx); //close the session  
                return 0;  
         }  
}  

