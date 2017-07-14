/*
* Test suite program based of libusb-0.1-compat testlibusb
* Copyright (c) 2013 Nathan Hjelm <hjelmn@mac.ccom>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <string.h>
#include "libusb.h"
#define msleep(msecs) nanosleep(&(struct timespec){msecs / 1000, (msecs * 1000000) % 1000000000UL}, NULL);

int verbose = 0;

static void print_endpoint_comp(const struct libusb_ss_endpoint_companion_descriptor *ep_comp)
{
	printf("      USB 3.0 Endpoint Companion:\n");
	printf("        bMaxBurst:        %d\n", ep_comp->bMaxBurst);
	printf("        bmAttributes:     0x%02x\n", ep_comp->bmAttributes);
	printf("        wBytesPerInterval: %d\n", ep_comp->wBytesPerInterval);
}

static void print_endpoint(const struct libusb_endpoint_descriptor *endpoint)
{
	int i, ret;

	printf("      Endpoint:\n");
	printf("        bEndpointAddress: %02xh\n", endpoint->bEndpointAddress);
	printf("        bmAttributes:     %02xh\n", endpoint->bmAttributes);
	printf("        wMaxPacketSize:   %d\n", endpoint->wMaxPacketSize);
	printf("        bInterval:        %d\n", endpoint->bInterval);
	printf("        bRefresh:         %d\n", endpoint->bRefresh);
	printf("        bSynchAddress:    %d\n", endpoint->bSynchAddress);

	for (i = 0; i < endpoint->extra_length;) {
		if (LIBUSB_DT_SS_ENDPOINT_COMPANION == endpoint->extra[i + 1]) {
			struct libusb_ss_endpoint_companion_descriptor *ep_comp;

			ret = libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint, &ep_comp);
			if (LIBUSB_SUCCESS != ret) {
				continue;
			}

			print_endpoint_comp(ep_comp);

			libusb_free_ss_endpoint_companion_descriptor(ep_comp);
		}

		i += endpoint->extra[i];
	}
}

static void print_altsetting(const struct libusb_interface_descriptor *interface)
{
	int i;

	printf("    Interface:\n");
	printf("      bInterfaceNumber:   %d\n", interface->bInterfaceNumber);
	printf("      bAlternateSetting:  %d\n", interface->bAlternateSetting);
	printf("      bNumEndpoints:      %d\n", interface->bNumEndpoints);
	printf("      bInterfaceClass:    %d\n", interface->bInterfaceClass);
	printf("      bInterfaceSubClass: %d\n", interface->bInterfaceSubClass);
	printf("      bInterfaceProtocol: %d\n", interface->bInterfaceProtocol);
	printf("      iInterface:         %d\n", interface->iInterface);

	for (i = 0; i < interface->bNumEndpoints; i++)
		print_endpoint(&interface->endpoint[i]);
}

static void print_2_0_ext_cap(struct libusb_usb_2_0_extension_descriptor *usb_2_0_ext_cap)
{
	printf("    USB 2.0 Extension Capabilities:\n");
	printf("      bDevCapabilityType: %d\n", usb_2_0_ext_cap->bDevCapabilityType);
	printf("      bmAttributes:       0x%x\n", usb_2_0_ext_cap->bmAttributes);
}

static void print_ss_usb_cap(struct libusb_ss_usb_device_capability_descriptor *ss_usb_cap)
{
	printf("    USB 3.0 Capabilities:\n");
	printf("      bDevCapabilityType: %d\n", ss_usb_cap->bDevCapabilityType);
	printf("      bmAttributes:       0x%x\n", ss_usb_cap->bmAttributes);
	printf("      wSpeedSupported:    0x%x\n", ss_usb_cap->wSpeedSupported);
	printf("      bFunctionalitySupport: %d\n", ss_usb_cap->bFunctionalitySupport);
	printf("      bU1devExitLat:      %d\n", ss_usb_cap->bU1DevExitLat);
	printf("      bU2devExitLat:      %d\n", ss_usb_cap->bU2DevExitLat);
}

static void print_bos(libusb_device_handle *handle)
{
	struct libusb_bos_descriptor *bos;
	int ret;

	ret = libusb_get_bos_descriptor(handle, &bos);
	if (0 > ret) {
		return;
	}
	
	printf("  Binary Object Store (BOS):\n");
	printf("    wTotalLength:       %d\n", bos->wTotalLength);
	printf("    bNumDeviceCaps:     %d\n", bos->bNumDeviceCaps);
		
	if(bos->dev_capability[0]->bDevCapabilityType == LIBUSB_BT_USB_2_0_EXTENSION) {
	
		struct libusb_usb_2_0_extension_descriptor *usb_2_0_extension;
	        ret =  libusb_get_usb_2_0_extension_descriptor(NULL, bos->dev_capability[0],&usb_2_0_extension);
	        if (0 > ret) {
		        return;
	        }
	        
                print_2_0_ext_cap(usb_2_0_extension);
                libusb_free_usb_2_0_extension_descriptor(usb_2_0_extension);
        }
	
	if(bos->dev_capability[0]->bDevCapabilityType == LIBUSB_BT_SS_USB_DEVICE_CAPABILITY) {
	
	        struct libusb_ss_usb_device_capability_descriptor *dev_cap;
		ret = libusb_get_ss_usb_device_capability_descriptor(NULL, bos->dev_capability[0],&dev_cap);
	        if (0 > ret) {
		        return;
	        }
	        
	        print_ss_usb_cap(dev_cap);
	        libusb_free_ss_usb_device_capability_descriptor(dev_cap);
        }
        
	libusb_free_bos_descriptor(bos);
}

static void print_interface(const struct libusb_interface *interface)
{
	int i;

	for (i = 0; i < interface->num_altsetting; i++)
		print_altsetting(&interface->altsetting[i]);
}

static void print_configuration(struct libusb_config_descriptor *config)
{
	int i;

	printf("  Configuration:\n");
	printf("    wTotalLength:         %d\n", config->wTotalLength);
	printf("    bNumInterfaces:       %d\n", config->bNumInterfaces);
	printf("    bConfigurationValue:  %d\n", config->bConfigurationValue);
	printf("    iConfiguration:       %d\n", config->iConfiguration);
	printf("    bmAttributes:         %02xh\n", config->bmAttributes);
	printf("    MaxPower:             %d\n", config->MaxPower);

	for (i = 0; i < config->bNumInterfaces; i++)
		print_interface(&config->interface[i]);
}

void func(void)
{
	struct libusb_device *dev;
	struct libusb_device_descriptor desc;
	libusb_device_handle *handle = NULL;
	
	struct libusb_config_descriptor *conf_desc;
	const struct libusb_endpoint_descriptor *endpoint;

	uint8_t bus, port_path[8];
	int size;

	uint8_t endpoint_in = 0, endpoint_out = 0;

	char send_buf[512] = {'c'};
	char recv_buf[512] = {0};
	char description[256];
	char string[256];
	int ret, i , j, k, r;

	int iface, nb_ifaces, first_iface = -1;

	uint8_t string_index[3];	// indexes of the string descriptors
	
	const char* speed_name[5] = { "Unknown", "1.5 Mbit/s (USB LowSpeed)", "12 Mbit/s (USB FullSpeed)",
		"480 Mbit/s (USB HighSpeed)", "5000 Mbit/s (USB SuperSpeed)"};
	

	handle = libusb_open_device_with_vid_pid(NULL, 0x05c6, 0x9999);
	if (handle == NULL) {
		printf("  libusb_open_device_with_vid_pid Failed.\n");
		return -1;
	}
	dev = libusb_get_device(handle);
	bus = libusb_get_bus_number(dev);

	r = libusb_get_port_numbers(dev, port_path, sizeof(port_path));
	if (r > 0) {
			printf("\nDevice properties:\n");
			printf("        bus number: %d\n", bus);
			printf("         port path: %d", port_path[0]);
			for (i=1; i<r; i++) {
				printf("->%d", port_path[i]);
			}
			printf(" (from root hub)\n");
	}
	r = libusb_get_device_speed(dev);
	if ((r<0) || (r>4)) r=0;
	printf("             speed: %s\n", speed_name[r]);

	printf("\nReading device descriptor:\n");
	libusb_get_device_descriptor(dev, &desc);
	printf("            length: %d\n", desc.bLength);
	printf("      device class: %d\n", desc.bDeviceClass);
	printf("               S/N: %d\n", desc.iSerialNumber);
	printf("           VID:PID: %04X:%04X\n", desc.idVendor, desc.idProduct);
	printf("         bcdDevice: %04X\n", desc.bcdDevice);
	printf("   iMan:iProd:iSer: %d:%d:%d\n", desc.iManufacturer, desc.iProduct, desc.iSerialNumber);
	printf("          nb confs: %d\n", desc.bNumConfigurations);

	// Copy the string descriptors for easier parsing
	string_index[0] = desc.iManufacturer;
	string_index[1] = desc.iProduct;
	string_index[2] = desc.iSerialNumber;
	
	printf("\nReading first configuration descriptor:\n");
	libusb_get_config_descriptor(dev, 0, &conf_desc);
	nb_ifaces = conf_desc->bNumInterfaces;
	printf("             nb interfaces: %d\n", nb_ifaces);


	//read interfaces and endpoint
	for (i = 0; i < conf_desc->bNumInterfaces; i++)
			print_interface(&conf_desc->interface[i]);
	
	r = libusb_claim_interface(handle, 3);
	printf("libusb_claim_interface return %d\n" , r);

	r = libusb_bulk_transfer(handle, 0x85, send_buf, sizeof(send_buf), NULL, 1000);
	printf("libusb_bulk_transfer return %d\n" , r);
	//usb_bulk_write(handle, 0x85, send_buf, sizeof(send_buf) , 5000);

	r = libusb_bulk_transfer(handle, 0x04, recv_buf, 1, NULL, 1000);
	printf("libusb_bulk_transfer return %d\n" , r);
	printf("recv buf %s\n" , recv_buf);

	//usb_bulk_read(handle, 0x04, recv_buf, sizeof(send_buf) , 5000);

	
	r = libusb_release_interface(handle, 3);
	printf("libusb_release_interface return %d\n" , r);
	
	/*
	if (nb_ifaces > 0)
		first_iface = conf_desc->usb_interface[0].altsetting[0].bInterfaceNumber;
	for (i=0; i<nb_ifaces; i++) {
		printf("              interface[%d]: id = %d\n", i,
			conf_desc->usb_interface[i].altsetting[0].bInterfaceNumber);
		for (j=0; j<conf_desc->usb_interface[i].num_altsetting; j++) {
			printf("interface[%d].altsetting[%d]: num endpoints = %d\n",
				i, j, conf_desc->usb_interface[i].altsetting[j].bNumEndpoints);
			printf("   Class.SubClass.Protocol: %02X.%02X.%02X\n",
				conf_desc->usb_interface[i].altsetting[j].bInterfaceClass,
				conf_desc->usb_interface[i].altsetting[j].bInterfaceSubClass,
				conf_desc->usb_interface[i].altsetting[j].bInterfaceProtocol);
			if ( (conf_desc->usb_interface[i].altsetting[j].bInterfaceClass == LIBUSB_CLASS_MASS_STORAGE)
			  && ( (conf_desc->usb_interface[i].altsetting[j].bInterfaceSubClass == 0x01)
			  || (conf_desc->usb_interface[i].altsetting[j].bInterfaceSubClass == 0x06) )
			  && (conf_desc->usb_interface[i].altsetting[j].bInterfaceProtocol == 0x50) ) {
				// Mass storage devices that can use basic SCSI commands
				//test_mode = USE_SCSI;
			}
			for (k=0; k<conf_desc->usb_interface[i].altsetting[j].bNumEndpoints; k++) {
				struct libusb_ss_endpoint_companion_descriptor *ep_comp = NULL;
				endpoint = &conf_desc->usb_interface[i].altsetting[j].endpoint[k];
				printf("       endpoint[%d].address: %02X\n", k, endpoint->bEndpointAddress);
				// Use the first interrupt or bulk IN/OUT endpoints as default for testing
				if ((endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) & (LIBUSB_TRANSFER_TYPE_BULK | LIBUSB_TRANSFER_TYPE_INTERRUPT)) {
					if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
						if (!endpoint_in)
							endpoint_in = endpoint->bEndpointAddress;
					} else {
						if (!endpoint_out)
							endpoint_out = endpoint->bEndpointAddress;
					}
				}
				printf("           max packet size: %04X\n", endpoint->wMaxPacketSize);
				printf("          polling interval: %02X\n", endpoint->bInterval);
				libusb_get_ss_endpoint_companion_descriptor(NULL, endpoint, &ep_comp);
				if (ep_comp) {
					printf("                 max burst: %02X   (USB 3.0)\n", ep_comp->bMaxBurst);
					printf("        bytes per interval: %04X (USB 3.0)\n", ep_comp->wBytesPerInterval);
					libusb_free_ss_endpoint_companion_descriptor(ep_comp);
				}
			}
		}
	}
	*/
	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		fprintf(stderr, "failed to get device descriptor");
		return -1;
	}
	//printf("desc.idProduct:%u  desc.idProduct:%u\n" ,desc.idVendor,desc.idProduct );
	if(desc.idVendor == 0x05c6 && desc.idProduct == 0x9999 )
	{
		printf("find devices\n");
		//do translate
		ret = libusb_open(dev, &handle);

		

		if(handle)
			libusb_close(handle);
	}
	return ;
}
static int print_device(libusb_device *dev, int level)
{
	struct libusb_device_descriptor desc;
	libusb_device_handle *handle = NULL;
	char description[256];
	char string[256];
	int ret, i;

	ret = libusb_get_device_descriptor(dev, &desc);
	if (ret < 0) {
		fprintf(stderr, "failed to get device descriptor");
		return -1;
	}

	ret = libusb_open(dev, &handle);
	if (LIBUSB_SUCCESS == ret) {
		if (desc.iManufacturer) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, string, sizeof(string));
			if (ret > 0){
				snprintf(description, sizeof(description), "%s - ", string);
				
			}
			else
				snprintf(description, sizeof(description), "%04X - ",
				desc.idVendor);
		}
		else
			snprintf(description, sizeof(description), "%04X - ",
			desc.idVendor);

		if (desc.iProduct) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iProduct, string, sizeof(string));
			if (ret > 0)
				snprintf(description + strlen(description), sizeof(description) -
				strlen(description), "%s", string);
			else
				snprintf(description + strlen(description), sizeof(description) -
				strlen(description), "%04X", desc.idProduct);
		}
		else
			snprintf(description + strlen(description), sizeof(description) -
			strlen(description), "%04X", desc.idProduct);
	}
	else {
		snprintf(description, sizeof(description), "%04X - %04X",
			desc.idVendor, desc.idProduct);
	}

	printf("%.*sDev (bus %d, device %d): %s\n", level * 2, "                    ",
		libusb_get_bus_number(dev), libusb_get_device_address(dev), description);

	if (handle && verbose) {
		if (desc.iSerialNumber) {
			ret = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, string, sizeof(string));
			if (ret > 0)
				printf("%.*s  - Serial Number: %s\n", level * 2,
				"                    ", string);
		}
	}

	if (verbose) {
		for (i = 0; i < desc.bNumConfigurations; i++) {
			struct libusb_config_descriptor *config;
			ret = libusb_get_config_descriptor(dev, i, &config);
			if (LIBUSB_SUCCESS != ret) {
				printf("  Couldn't retrieve descriptors\n");
				continue;
			}

			print_configuration(config);

			libusb_free_config_descriptor(config);
		}

                
		if (handle && desc.bcdUSB >= 0x0201) {
			print_bos(handle);
		}
	}

	if (handle)
		libusb_close(handle);

	return 0;
}
void test()
{
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
        printf("Get Device Error\n");   //there was an error  
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
		/*
		if(libusb_kernel_driver_active(dev_handle, 3) == 1) { //find out if kernel driver is attached  
            printf("Kernel Driver Active\n");  
            if(libusb_detach_kernel_driver(dev_handle, 3) == 0) //detach it  
                printf("Kernel Driver Detached!\n");  
        }  
        */
        r = libusb_claim_interface(dev_handle, 3);            //claim interface 0 (the first) of device (mine had jsut 1)  
        if(r < 0) {  
            printf("Cannot Claim Interface\n");  
            return 1;  
        }  
        printf("Claimed Interface\n");  
        int size;  
        unsigned char read_buf[1024*1024*2]="\0"; 
		unsigned char send_buf[1024]="\0";
		struct timeval  old_time , current_time;
		gettimeofday( &old_time, NULL );
		static unsigned long count = 0;
		while(1)
		//for(int i=0;i<3;i++)  
        {  
        	int i = 0;
        	size =  0;
			int rr = 0 ; 
			/*
        	memset(send_buf , 'A' + i , sizeof(send_buf));
			memset(datain , 0  , sizeof(datain));
        	rr = libusb_bulk_transfer(dev_handle,  
                    0x04,  
                    send_buf,  
                    1024,  
                    &size,  
                    1000);  
            printf("libusb_bulk_transfer rr: %d \n" , rr);
			printf("libusb_bulk_transfer rr: %d \n" , rr);  
            printf("size: %d\n" ,size);  
            printf("data: send ");  
            for(int j=0; j<size; j++)  
                  printf("%02x ", (unsigned char)(send_buf[j]));  
            printf("\n\n\n");
			
			msleep(1000);
			*/
            rr = libusb_bulk_transfer(dev_handle,  
                    0x85,  
                  read_buf,  
                    sizeof(read_buf),//1024*1024,  
                    &size,  
                    1000); 
			count = size + count;
			gettimeofday( &current_time, NULL );
			if((1000000 * ( current_time.tv_sec - old_time.tv_sec ) + current_time.tv_usec - old_time.tv_usec) > 1000000 ){
				printf("count:%lu ----\n" , count/1024);
				count = 0;
				old_time = current_time;
				/*
				printf("libusb_bulk_transfer rr: %d \n" , rr);  
            	printf("size: %d\n" ,size);  
            	printf("data:  recv");
            	for(int j=0; j<size; j++)  
                  	printf("%02x", (unsigned char)(read_buf[j]));  
            		printf("\n");
            	*/
			}
			/*
            printf("libusb_bulk_transfer rr: %d \n" , rr);  
            printf("size: %d\n" ,size);  
            printf("data:  recv");  
            for(int j=0; j<size; j++)  
                  printf("%02x", (unsigned char)(read_buf[j]));  
            printf("\n");
            */
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
int main(int argc, char *argv[])
{
	test();
	/*
	printf("hellow\n");
	libusb_device **devs;
	ssize_t cnt;
	int r, i;

	if (argc > 1 && !strcmp(argv[1], "-v"))
		verbose = 1;
	printf("hellow\n");
	r = libusb_init(NULL);
	if (r < 0)
		return r;

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
		return (int)cnt;

	func();
	
	for (i = 0; devs[i]; ++i) {
		//func(devs[i]);
		//print_device(devs[i], 0);
	}

	libusb_free_device_list(devs, 1);

	libusb_exit(NULL);
	return 0;
	*/
}
