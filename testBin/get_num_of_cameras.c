#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>  
#include <string.h>
#include <stdio.h>
#include <unistd.h>
//#include <sys/system_properties.h>

#include "get_num_of_cameras.h"
//#include <linux/media.h>

int main()
{
    int rc = 0;
    int dev_fd = -1;
    struct media_device_info mdev_info;
    int num_media_devices = 0;
    int8_t num_cameras = 0;
    char subdev_name[32];
    int32_t sd_fd = -1;

    while (1) {
        uint32_t num_entities = 1U;
        char dev_name[32];

        snprintf(dev_name, sizeof(dev_name), "/dev/media%d", num_media_devices);
        dev_fd = open(dev_name, O_RDWR | O_NONBLOCK);
        if (dev_fd < 0) {
            printf("Done discovering media devices,%s\n",dev_name);
            break;
        }
        num_media_devices++;
        rc = ioctl(dev_fd, MEDIA_IOC_DEVICE_INFO, &mdev_info);
        if (rc < 0) {
            printf("Error: ioctl media_dev failed: %s\n", strerror(errno));
            close(dev_fd);
            dev_fd = -1;
            break;
        }

        if (strncmp(mdev_info.model, MSM_CONFIGURATION_NAME,
          sizeof(mdev_info.model)) != 0) {
            close(dev_fd);
            dev_fd = -1;
            continue;
        }

        while (1) {
            struct media_entity_desc entity;
            memset(&entity, 0, sizeof(entity));
            entity.id = num_entities++;
            printf("entity id %d\n", entity.id);
            rc = ioctl(dev_fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
            if (rc < 0) {
                printf("Done enumerating media entities\n");
                rc = 0;
                break;
            }
            printf("entity name %s type %d group id %d\n",
                entity.name, entity.type, entity.group_id);
            if (entity.type == MEDIA_ENT_T_V4L2_SUBDEV &&
                entity.group_id == MSM_CAMERA_SUBDEV_SENSOR_INIT) {
                snprintf(subdev_name, sizeof(dev_name), "/dev/%s", entity.name);
				printf("subdev_name:%s\n",subdev_name);
                break;
            }
        }
        close(dev_fd);
        dev_fd = -1;
    }
#if 0
    /* Open sensor_init subdev */
    sd_fd = open(subdev_name, O_RDWR);
    if (sd_fd < 0) {
        printf("Open sensor_init subdev failed");
        return FALSE;
    }

    cfg.cfgtype = CFG_SINIT_PROBE_WAIT_DONE;
    cfg.cfg.setting = NULL;
    if (ioctl(sd_fd, VIDIOC_MSM_SENSOR_INIT_CFG, &cfg) < 0) {
        printf("failed");
    }
    close(sd_fd);
    dev_fd = -1;
#endif

#if 1
    num_media_devices = 0;
    while (1) {
        uint32_t num_entities = 1U;
        char dev_name[32];

        snprintf(dev_name, sizeof(dev_name), "/dev/media%d", num_media_devices);
        dev_fd = open(dev_name, O_RDWR | O_NONBLOCK);
        if (dev_fd < 0) {
            printf("Done discovering media devices: %s\n", strerror(errno));
            break;
        }
        num_media_devices++;
        memset(&mdev_info, 0, sizeof(mdev_info));
        rc = ioctl(dev_fd, MEDIA_IOC_DEVICE_INFO, &mdev_info);
        if (rc < 0) {
            printf("Error: ioctl media_dev failed: %s\n", strerror(errno));
            close(dev_fd);
            dev_fd = -1;
            num_cameras = 0;
            break;
        }
		printf("------driver:%s model:%s serial:%s bus_info:%s------\n"
			,mdev_info.driver
			,mdev_info.model
			,mdev_info.serial
			,mdev_info.bus_info
			);

        if(strncmp(mdev_info.model, MSM_CAMERA_NAME, sizeof(mdev_info.model)) != 0) {
            close(dev_fd);
            dev_fd = -1;
            continue;
        }

        while (1) {
            struct media_entity_desc entity;
            memset(&entity, 0, sizeof(entity));
            entity.id = num_entities++;
            rc = ioctl(dev_fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
            if (rc < 0) {
                printf("Done enumerating media entities\n");
                rc = 0;
                break;
            }
            if(entity.type == MEDIA_ENT_T_DEVNODE_V4L && entity.group_id == QCAMERA_VNODE_GROUP_ID) {
                //strlcpy(g_cam_ctrl.video_dev_name[num_cameras],
                //     entity.name, sizeof(entity.name));
                printf("dev_info[id=%d,name='%s']\n",
                    (int)num_cameras, entity.name);
                num_cameras++;
                break;
            }
        }
        close(dev_fd);
        dev_fd = -1;
        if (num_cameras >= MM_CAMERA_MAX_NUM_SENSORS) {
            printf("Maximum number of camera reached %d", num_cameras);
            break;
        }
    }
    //g_cam_ctrl.num_cam = num_cameras;

    //get_sensor_info();
    //sort_camera_info(g_cam_ctrl.num_cam);
    /* unlock the mutex */
    //pthread_mutex_unlock(&g_intf_lock);
    printf("num_cameras=%d\n", (int)num_cameras);
    return num_cameras;
#endif
}
