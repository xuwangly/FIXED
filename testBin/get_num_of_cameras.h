#ifndef GET_NUM_OF_CAMERAS
#define GET_NUM_OF_CAMERAS

#define QCAMERA_DEVICE_GROUP_ID	1
#define QCAMERA_VNODE_GROUP_ID	2
#define MM_CAMERA_MAX_NUM_SENSORS  5
#define MSM_CONFIGURATION_NAME	"msm_config"
#define MSM_CAMERA_NAME		"msm_camera"

#define PROPERTY_VALUE_MAX 40

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;


struct media_device_info {
	char driver[16];
	char model[32];
	char serial[40];
	char bus_info[32];
	__u32 media_version;
	__u32 hw_revision;
	__u32 driver_version;
	__u32 reserved[31];
};

struct media_entity_desc {
	__u32 id;
	char name[32];
	__u32 type;
	__u32 revision;
	__u32 flags;
	__u32 group_id;
	__u16 pads;
	__u16 links;

	__u32 reserved[4];

	union {
		/* Node specifications */
		struct {
			__u32 major;
			__u32 minor;
		} v4l;
		struct {
			__u32 major;
			__u32 minor;
		} fb;
		struct {
			__u32 card;
			__u32 device;
			__u32 subdevice;
		} alsa;
		int dvb;

		/* Sub-device specifications */
		/* Nothing needed yet */
		__u8 raw[184];
	};
};

#define MEDIA_IOC_DEVICE_INFO		_IOWR('|', 0x00, struct media_device_info)
#define MEDIA_IOC_ENUM_ENTITIES		_IOWR('|', 0x01, struct media_entity_desc)
#define MEDIA_IOC_ENUM_LINKS		_IOWR('|', 0x02, struct media_links_enum)
#define MEDIA_IOC_SETUP_LINK		_IOWR('|', 0x03, struct media_link_desc)

#define MEDIA_ENT_ID_FLAG_NEXT		(1 << 31)

#define MEDIA_ENT_TYPE_SHIFT		16
#define MEDIA_ENT_TYPE_MASK		0x00ff0000
#define MEDIA_ENT_SUBTYPE_MASK		0x0000ffff

#define MEDIA_ENT_T_DEVNODE		(1 << MEDIA_ENT_TYPE_SHIFT)
#define MEDIA_ENT_T_DEVNODE_V4L		(MEDIA_ENT_T_DEVNODE + 1)
#define MEDIA_ENT_T_DEVNODE_FB		(MEDIA_ENT_T_DEVNODE + 2)
#define MEDIA_ENT_T_DEVNODE_ALSA	(MEDIA_ENT_T_DEVNODE + 3)
#define MEDIA_ENT_T_DEVNODE_DVB		(MEDIA_ENT_T_DEVNODE + 4)

#define MEDIA_ENT_T_V4L2_SUBDEV		(2 << MEDIA_ENT_TYPE_SHIFT)
#define MEDIA_ENT_T_V4L2_SUBDEV_SENSOR	(MEDIA_ENT_T_V4L2_SUBDEV + 1)
#define MEDIA_ENT_T_V4L2_SUBDEV_FLASH	(MEDIA_ENT_T_V4L2_SUBDEV + 2)
#define MEDIA_ENT_T_V4L2_SUBDEV_LENS	(MEDIA_ENT_T_V4L2_SUBDEV + 3)

#define MSM_CAMERA_SUBDEV_CSIPHY       0
#define MSM_CAMERA_SUBDEV_CSID         1
#define MSM_CAMERA_SUBDEV_ISPIF        2
#define MSM_CAMERA_SUBDEV_VFE          3
#define MSM_CAMERA_SUBDEV_AXI          4
#define MSM_CAMERA_SUBDEV_VPE          5
#define MSM_CAMERA_SUBDEV_SENSOR       6
#define MSM_CAMERA_SUBDEV_ACTUATOR     7
#define MSM_CAMERA_SUBDEV_EEPROM       8
#define MSM_CAMERA_SUBDEV_CPP          9
#define MSM_CAMERA_SUBDEV_CCI          10
#define MSM_CAMERA_SUBDEV_LED_FLASH    11
#define MSM_CAMERA_SUBDEV_STROBE_FLASH 12
#define MSM_CAMERA_SUBDEV_BUF_MNGR     13
#define MSM_CAMERA_SUBDEV_SENSOR_INIT  14
#define MSM_CAMERA_SUBDEV_OIS          15
#define MSM_CAMERA_SUBDEV_FLASH        16
#define MSM_CAMERA_SUBDEV_EXT          17
#if 0
struct sensor_init_cfg_data {
	enum msm_sensor_init_cfg_type_t cfgtype;
	struct msm_sensor_info_t        probed_info;
	char                            entity_name[MAX_SENSOR_NAME];
	union {
		void *setting;
	} cfg;
};
#endif
#endif
