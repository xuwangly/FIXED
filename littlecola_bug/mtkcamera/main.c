/* =============================================================================
* Copyright (c) 2013-2014 MM Solutions AD
* All rights reserved. Property of MM Solutions AD.
*
* This source code may not be used against the terms and conditions stipulated
* in the licensing agreement under which it has been supplied, or without the
* written permission of MM Solutions. Rights to use, copy, modify, and
* distribute or disclose this source code and its documentation are granted only
* through signed licensing agreement, provided that this copyright notice
* appears in all copies, modifications, and distributions and subject to the
* following conditions:
* THIS SOURCE CODE AND ACCOMPANYING DOCUMENTATION, IS PROVIDED AS IS, WITHOUT
* WARRANTY OF ANY KIND, EXPRESS OR IMPLIED. MM SOLUTIONS SPECIFICALLY DISCLAIMS
* ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN
* NO EVENT SHALL MM SOLUTIONS BE LIABLE TO ANY PARTY FOR ANY CLAIM, DIRECT,
* INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
* PROFITS, OR OTHER LIABILITY, ARISING OUT OF THE USE OF OR IN CONNECTION WITH
* THIS SOURCE CODE AND ITS DOCUMENTATION.
* =========================================================================== */
/**
* @file
*
* @author ( MM Solutions AD )
*
*/
/* -----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*! 05-Nov-2013 : Author ( MM Solutions AD )
*! Created
* =========================================================================== */
#include "rtems_config.h"
#include <rtems/shell.h>

#include <stdint.h>
#include <osal/osal_stdlib.h>
#include <osal/osal_time.h>
#include <utils/mms_debug.h>

#include <version_info.h>

#include <pool_bm.h>

#include <platform/inc/platform.h>

#include <guzzi_event/include/guzzi_event.h>
#include <guzzi_event_global/include/guzzi_event_global.h>

#include <dtp/dtp_server_defs.h>

#include <camera_config_struct.h>

#include <components/camera/vcamera_iface/virt_cm/inc/virt_cm.h>

#include "initSystem.h"
#include "camera_control.h"
#include "app_guzzi_command_spi.h"
#include "app_guzzi_command_dbg.h"

#include "PlgWarpStitch3StillApi.h"
#include "DrvTempSensor.h"

// Enable readig the internal thermal sensor
//#define MEASURE_INT_TEMP
// Temperature sample interval in multiples of 10mS (100 * 10mS)
#define TEMP_SAMPLE_INTERVAL    (100)

/*
#include <stdlib.h>
#include <stdio.h>
#include <rtems.h>
#include <rtems/libio.h>
//#include <OsBrdMv0182.h>
//#include <app_config.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <rtems/status-checks.h>
#include <OsSpiSlave.h>
#include <DrvGpio.h>*/


dtp_server_hndl_t dtp_srv_hndl;
extern uint8_t ext_dtp_database[];
extern uint8_t ext_dtp_database_end[];

extern uint32_t dbgEnableOutput;

extern uint32_t capture_busy;

extern uint32_t lrt_stitch_mode;

extern guzzi_camera3_enum_z_custom_usecase_selection_t get_cust_usecase(int cam_id);

mmsdbg_define_variable(
        vdl_guzzi_i2c,
        DL_DEFAULT,
        0,
        "vdl_guzzi_i2c",
        "Test android control."
    );
#define MMSDEBUGLEVEL mmsdbg_use_variable(vdl_guzzi_i2c)

/*
 * ****************************************************************************
 * ** Temp functions referd by guzzi lib **************************************
 * ****************************************************************************
 */
void guzzi_camera3_capture_result__x11_configure_streams(
        int camera_id,
        void *streams
    )
{
    UNUSED(camera_id);
    UNUSED(streams);
}

void guzzi_camera3_capture_result(
        int camera_id,
        unsigned int stream_id,
        unsigned int frame_number,
        void *data,
        unsigned int data_size
    )
{
    UNUSED(camera_id);
    UNUSED(stream_id);
    UNUSED(frame_number);
    UNUSED(data);
    UNUSED(data_size);
}

/*
 * ****************************************************************************
 * ** Profile callback  *******************************************************
 * ****************************************************************************
 */
static void profile_ready_cb(
        profile_t *profile,
        void *prv,
        void *buffer,
        unsigned int buffer_size
    )
{
    UNUSED(profile);
    UNUSED(prv);
    UNUSED(buffer_size);
    //printf(">>> prof: addr=%#010x size=%d\n", buffer, buffer_size);
    PROFILE_RELEASE_READY(buffer);
}

/*
 * ****************************************************************************
 * ** App GUZZI Command callback **********************************************
 * ****************************************************************************
 */
static void app_guzzi_command_callback(
        void *app_private,
        app_guzzi_command_t *command
    )
{
    uint32_t st_cam_0 = 0;
    uint32_t st_cam_1 = 0;
    uint32_t st_cam_2 = 0;

    UNUSED(app_private);
    mmsdbg(DL_ERROR, "command->id:%d", command->id);

    // Get the sctive state for all cameras
    st_cam_0 = camera_control_is_active(0);
    st_cam_1 = camera_control_is_active(1);
    st_cam_2 = camera_control_is_active(2);

    // Skip Start command if any of the cameras is already active
    if (st_cam_0 || st_cam_1 || st_cam_2) {
        if (APP_GUZZI_COMMAND__CAM_START == command->id) {
            mmsdbg(DL_ERROR, "Skipping START command!");
            return;
        }
    } 

    // Skip commands unless all cameras are active
    if (!(st_cam_0 && st_cam_1 && st_cam_2)) {
        // Alow transition to LP, use case switch and start
        if ((APP_GUZZI_COMMAND__CAM_START != command->id) &&
            (APP_GUZZI_COMMAND__CAM_LP_MODE != command->id) && 
            (APP_GUZZI_COMMAND__USECASE_STILL_FULL_SIZE != command->id) &&
            (APP_GUZZI_COMMAND__USECASE_VDO_BINNING_SIZE != command->id)) {
            mmsdbg(DL_ERROR, "Skipping command %d for Non active camera", command->id);
            return;
        }
    }

    switch (command->id) {
        case APP_GUZZI_COMMAND__NOP:
            //camera_control_start(0);
            break;
        case APP_GUZZI_COMMAND__CAM_START:
            PROFILE_ADD(PROFILE_ID_EXT_START_CMD, 0, 0);
            // Delay for Host to get ready
            osal_usleep(150 * 1000);
            // Execute CAM_START command for all 3 cameras
            camera_control_start(0);
            camera_control_start(1);
            camera_control_start(2);
            mmsdbg(DL_ERROR, "exec command %d for all\n", command->id);
			dbgEnableOutput = 1; //(1 << command->cam.id);
            break;
        case APP_GUZZI_COMMAND__CAM_STOP:
            PROFILE_ADD(PROFILE_ID_EXT_STOP_CMD, 0, 0);
            // Execute CAM_STOP command for all 3 cameras
            camera_control_stop(0);
            camera_control_stop(1);
            camera_control_stop(2);
            mmsdbg(DL_ERROR, "exec command %d for all\n", command->id);
            break;
        case APP_GUZZI_COMMAND__CAM_CAPTURE:
            if (get_cust_usecase(0) != GUZZI_CAMERA3_ENUM_Z_CUSTOM_USECASE_SELECTION_LOW_POWER_VIDEO) {
                // Check if the Still is not busy before sending the command
                if (!capture_busy) {
                    capture_busy = 1;
                    PROFILE_ADD(PROFILE_ID_EXT_CAPTURE_CMD, 0, 0);
                    camera_control_capture(0);
                    camera_control_capture(1);
                    camera_control_capture(2);
                    lrt_stitch_mode = MODE_STITCH_ALL;
                    mmsdbg(DL_ERROR, "exec capture \"%d\"\n", command->id);
                } else {
                    mmsdbg(DL_ERROR, "capture busy - skip\n");
                }
            } else {
                mmsdbg(DL_ERROR, "No capture in this mode - skip\n");
            }
            break;
        case APP_GUZZI_COMMAND__SINGLE_CAM_CAPTURE:
            if (get_cust_usecase(0) != GUZZI_CAMERA3_ENUM_Z_CUSTOM_USECASE_SELECTION_LOW_POWER_VIDEO) {
                // Check if the Still is not busy before sending the command
                if (!capture_busy) {
                    capture_busy = 1;
                    PROFILE_ADD(PROFILE_ID_EXT_CAPTURE_CMD, 0, 0);
                    lrt_stitch_mode++;
                    if (lrt_stitch_mode > 2) lrt_stitch_mode = 0;
                    camera_control_capture(0);
                    camera_control_capture(1);
                    camera_control_capture(2);
                    mmsdbg(DL_ERROR, "exec capture \"%d\"\n", command->id);
                } else {
                    mmsdbg(DL_ERROR, "capture busy - skip\n");
                }
            } else {
                mmsdbg(DL_ERROR, "No capture in this mode - skip\n");
            }
            break;
        case APP_GUZZI_COMMAND__CAM_LENS_MOVE:
            PROFILE_ADD(PROFILE_ID_EXT_LENS_MOVE, 0, 0);
            camera_control_lens_move(
                    command->cam.id,
                    command->cam.lens_move.pos
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.lens_move.pos);
            break;
        case APP_GUZZI_COMMAND__CAM_AF_TRIGGER:
            PROFILE_ADD(PROFILE_ID_EXT_LENS_MOVE, 0, 0);
            camera_control_focus_trigger(command->cam.id);
            mmsdbg(DL_ERROR, "command \"%d %lu\" sent\n", command->id, command->cam.id);
            break;
        case APP_GUZZI_COMMAND__CAM_AE_MANUAL:
            camera_control_ae_manual(
                    command->cam.id,
                    command->cam.ae_manual.exp_us,
                    command->cam.ae_manual.sensitivity_iso,
                    command->cam.ae_manual.frame_duration_us
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.ae_manual.exp_us,
                    command->cam.ae_manual.sensitivity_iso, command->cam.ae_manual.frame_duration_us);
            break;
        case APP_GUZZI_COMMAND__CAM_AE_AUTO:
            camera_control_ae_auto(
                    command->cam.id,
                    CAMERA_CONTROL__AE_AUTO__FLASH_MODE__AUTO
                );
            mmsdbg(DL_ERROR, "command \"%d %lu\" sent\n", command->id, command->cam.id);
            break;
        case APP_GUZZI_COMMAND__CAM_AWB_MODE:
            camera_control_awb_mode(
                    command->cam.id,
                    command->cam.awb_mode.mode
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.awb_mode.mode);
            break;
        case APP_GUZZI_COMMAND__CAM_SCENE_MODE:
            camera_control_scene_mode(
                    command->cam.id,
                    command->cam.scene_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.scene_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_ANTIBANDING_MODE:
            camera_control_antibanding_mode(
                    command->cam.id,
                    command->cam.antibanding_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.antibanding_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_AE_LOCK:
            camera_control_ae_lock_mode(
                    command->cam.id,
                    command->cam.ae_lock_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.ae_lock_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_AE_TARGET_FPS_RANGE:
            camera_control_ae_target_fps_range(
                    command->cam.id,
                    command->cam.ae_target_fps_range.min_fps,
                    command->cam.ae_target_fps_range.max_fps
                );
        mmsdbg(DL_ERROR, "command \"%d %lu %lu %lu\" sent\n", command->id,
                command->cam.id, command->cam.ae_target_fps_range.min_fps,
                command->cam.ae_target_fps_range.max_fps);
            break;
        case APP_GUZZI_COMMAND__CAM_AWB_LOCK:
            camera_control_awb_lock_mode(
                    command->cam.id,
                    command->cam.awb_lock_control.type
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.awb_lock_control.type);
            break;
        case APP_GUZZI_COMMAND__CAM_CAPTURE_INTRENT:
            camera_control_capture_intent(
                    command->cam.id,
                    command->cam.capture_intent.mode
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.capture_intent.mode);
            break;
        case APP_GUZZI_COMMAND__CAM_CONTROL_MODE:
            camera_control_mode(
                    command->cam.id,
                    command->cam.control_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.control_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_FRAME_DURATION:
            camera_control_frame_duration(
                    command->cam.id,
                    command->cam.frame_duration.val
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %llu\" sent\n",
                    command->id, command->cam.id, command->cam.frame_duration.val);
            break;
        case APP_GUZZI_COMMAND__CAM_AE_EXPOSURE_COMPENSATION:
            camera_control_exp_compensation(
                    command->cam.id,
                    command->cam.exposure_compensation.val
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.exposure_compensation.val);
            break;
        case APP_GUZZI_COMMAND__CAM_SENSITIVITY:
            camera_control_sensitivity(
                    command->cam.id,
                    command->cam.sensitivity.iso_val
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.sensitivity.iso_val);
            break;
        case APP_GUZZI_COMMAND__CAM_EFFECT_MODE:
            camera_control_effect_mode(
                    command->cam.id,
                    command->cam.effect_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.effect_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_AF_MODE:
            camera_control_af_mode(
                    command->cam.id,
                    command->cam.af_mode.type
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.af_mode.type);
            break;
        case APP_GUZZI_COMMAND__CAM_NOISE_REDUCTION_STRENGTH:
            camera_control_noise_reduction_strength(
                    command->cam.id,
                    command->cam.noise_reduction_strength.val
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.noise_reduction_strength.val);
            break;
        case APP_GUZZI_COMMAND__CAM_SATURATION:
            camera_control_saturation(
                    command->cam.id,
                    command->cam.saturation.val
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.saturation.val);
            break;
        case APP_GUZZI_COMMAND__CAM_BRIGHTNESS:
            camera_control_brightness(
                    command->cam.id,
                    command->cam.brightness.val
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.brightness.val);
            break;
        case APP_GUZZI_COMMAND__CAM_FORMAT:
            camera_control_format(
                    command->cam.id,
                    command->cam.format.val
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.format.val);
            break;
        case APP_GUZZI_COMMAND__CAM_RESOLUTION:
            camera_control_resolution(
                    command->cam.id,
                    command->cam.resolution.width,
                    command->cam.resolution.height
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu %lu\" sent\n", command->id,
                    command->cam.id, command->cam.resolution.width,
                    command->cam.resolution.height);
            break;
        case APP_GUZZI_COMMAND__CAM_SHARPNESS:
            camera_control_sharpness(
                    command->cam.id,
                    command->cam.sharpness.val
                );
            mmsdbg(DL_ERROR, "command \"%d %lu %lu\" sent\n",
                    command->id, command->cam.id, command->cam.sharpness.val);
            break;
        case APP_GUZZI_COMMAND__CAM_LP_MODE://36
            // Execute CAM_STOP command for all 3 cameras
            //camera_control_stop(0);
            //camera_control_stop(1);
            //camera_control_stop(2);
            // Enter LP mode
            //camera_control_lp_mode();
            //osal_usleep(100 * 1000);
            break;
        case APP_GUZZI_COMMAND__USECASE_STILL_FULL_SIZE://38
            camera_control_cust_usecase(0, 0);
            break;
        case APP_GUZZI_COMMAND__USECASE_VDO_BINNING_SIZE://39
	    printf("hellow world\n");
            camera_control_cust_usecase(0, 2);
            break;
	case APP_GUZZI_COMMAN__GET_TEMP://40
		printf("hellow world\n");
			break;
        default:
            mmsdbg(DL_ERROR, "Unknown App GUZZI Command: %d", command->id);
    }
}

/*
 * ****************************************************************************
 * ** Temp observe function ***************************************************
 * ****************************************************************************
 */
/* TODO: Implement this in board/platform dependent part  */
int app_guzzi_command_wait_timeout(
        void *app_private,
        app_guzzi_command_callback_t *callback,
        uint32_t timeout_ms
    )
{
    return app_guzzi_command_spi_wait_timeout(app_private, callback, timeout_ms)
         + app_guzzi_command_dbg_peek(app_private, callback);
}

/*
 * ****************************************************************************
 * ** RTEMS Shell           ***************************************************
 * ****************************************************************************
 */
void app_guzzi_rtems_shell_init()
{
    int rc = rtems_shell_init(
                    "SHLL", /* task name */
                    RTEMS_MINIMUM_STACK_SIZE * 2, /* task stack size */
                    200, /* task priority */
                    "/dev/usb0", /* device name */
                    false, /* run forever */
                    false, /* wait for shell to terminate */
                    NULL /* login check function,
                    use NULL to disable a login check */
    );
    if (rc != RTEMS_SUCCESSFUL)
    {
        printf("Shell initialization error\n");
        exit(rc);
    }
}
extern void notify_boot_ready(void);

char txBuff[4] = {0x00,0x01,0x02,0x03};//{0x4D, 0x43, 0x77, 0x66, 0x55,
                   // 0x44, 0x44, 0x45};//sbuf[0]=0x434D; //MAGIC Number 
                    								//sbuf[3]=0x4544; //Tail Number;
/*
 * ****************************************************************************
 * ** Main ********************************************************************
 * ****************************************************************************
 */
#define  MEASURE_INT_TEMP
void POSIX_Init (void *args)
{
    UNUSED(args);
#ifdef MEASURE_INT_TEMP
    uint32_t tc = 0;
    fp32 temp;
#endif

    //mmsdbg(DL_ERROR, "main E");
    version_info_init();

    initSystem();
    app_guzzi_rtems_shell_init();

    osal_init();
    pool_bm_init();
    dtpsrv_create(&dtp_srv_hndl);
    dtpsrv_import_db(
            dtp_srv_hndl,
            ext_dtp_database,
            ext_dtp_database_end - ext_dtp_database
        );
    PROFILE_INIT(4096, 2, profile_ready_cb, NULL);

    guzzi_platform_init();
    guzzi_event_global_ctreate();

    virt_cm_detect();
	
    app_guzzi_command_spi_init(); /* TODO: move to board/platform dependent part */
    extern void spi_slave_init(void);
	//spi_slave_init();
    // Set initial usecase
    store_cust_usecase(0, 0);
    store_cust_usecase(1, 0);
    store_cust_usecase(2, 0);

    for (;;) {
        app_guzzi_command_wait_timeout(
                NULL,
                app_guzzi_command_callback,
                10
            );
//#define  MEASURE_INT_TEMP
#ifdef MEASURE_INT_TEMP
        if (TEMP_SAMPLE_INTERVAL <= tc++) {
            DrvTempSensorGetTemp(&temp);
		extern void temp_transfer_to_AP(char temp1 , char temp2);
		char temp1 = temp*100/100;
		char temp2 = ((int)(temp*100))%100;
		void get_temp(void);
		txBuff[0] = temp1 ;
		txBuff[3] = temp2 ;
		//get_temp();
		//temp_transfer_to_AP(temp1 , temp2 );
		extern void notify_boot_ready(void);
		//notify_boot_ready();
            printf("temp = %d C \n", txBuff[0]*100 + txBuff[3]);
            tc = 0;
        }
#endif
    }

}
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
char rxBuff[10] = {0};

void get_temp(void)
{
	int fd;

    /*/// Open the virtual channel
    fd = open("/dev/spiSlave", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spiSlave0", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spiSlave1", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spiSlave2", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spiSlave3", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spiSlave4", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spiSlave5", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spiSlave6", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spiSlave7", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spiSlave8", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spislave", O_RDWR);
	printf("ret fd:%d\n" , fd);
	fd = open("/dev/spislave0", O_RDWR);
	printf("ret fd:%d\n" , fd);*/
	fd = open("/dev/app_guzzi_command_spi", O_RDWR);//,
              //S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	printf("ret fd:%d\n" , fd);
    if(fd < 0)
    {
        perror("ERROR openning spi - closing thread (spi memory dump) ! \n");
        //exit(0);
        #include <errno.h> 
		printf("errno:%d\n" , errno);
		return;
    }
        /*struct spiSlaveTransfer_t spiRxTx;
        int i;
        int rval;

        spiRxTx.rxBuffer = rxBuff;
        spiRxTx.txBuffer = txBuff;
        spiRxTx.size = sizeof(rxBuff);

        rval = ioctl(fd, SPI_SLAVE_RXTX, &spiRxTx);
        if(rval < 0)
        {
            perror("ERROR reading spi - closing thread (spi memory dump) !");
            printf(" %x \n", rval);
        }

        printf("transfer done  !!!!!!!!!!!!!!!\n");

        for(i = 0; i < sizeof(rxBuff) ; i++)
        {
            printf("0x%X ", rxBuff[i] & 0xFF);
        }
        printf("\n");*/
        int mi , mk ;
		for( mi = 0 ; mi < 1000 ; mi++)
			for( mk = 0 ; mk < 1000 ; mk++)
				;
        int rval = write(fd, txBuff, sizeof(txBuff));
        if(rval != sizeof(txBuff))
        {
            printf("ERROR reading spi - closing thread (spi memory dump) ! \n");
        }

        printf(" TX TX TX TX TX TX transfer done  !!!!!!!!!!!!!!!\n");
	close(fd);
}
void lrt_func(void)
{
	printf("temp : %d\n" , txBuff[3]*100 + txBuff[4]);
	return;
}

