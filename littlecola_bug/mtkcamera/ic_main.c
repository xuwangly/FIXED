///
/// @file
/// @copyright All code copyright Movidius Ltd 2014, all rights reserved.
///            For License Warranty see: common/license.txt
///
/// @brief     Test app main()
///

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <assert.h>

#include <ipipe.h>

#include <utils/mms_debug.h>
#include <utils/profile/profile.h>
#include <osal/osal_stdlib.h>
#include <osal/osal_mutex.h>
#include <osal/pool.h>

#include <camera_config_struct.h>

#include "stitchingPipeline_lrt.h"
#include "stitchingPipelineStill_lrt.h"

#include <registersMyriad.h>
#include <DrvRegUtils.h>
#include <DrvMipi.h>
#include <DrvMss.h>
#include <DrvGpio.h>
#include <pool_bm.h>
#include <mipi.h>
#include <mipi_tx_module.h>
#include <mipi_tx_board_params.h>
#include "IspCommon.h"
#include <mipi_tx_thread.h>
#include <mipi_tx_header.h>
#include <mipi_tx_client.h>
#include "ipipeUtils.h"
#include <sendOutApi.h>
#include "LcdCEA1080p60.h"
#include <brdMv0182.h>

mmsdbg_define_variable(
        vdl_ic,
        DL_DEFAULT,
        0,
        "vdl_ic",
        "Guzzi IC."
    );
#define MMSDEBUGLEVEL mmsdbg_use_variable(vdl_ic)

// Scenarios

#define INSTANCES_COUNT_MAX     3

#define IC_IMAGE_WIDTH           (CAM_IMG_FULL_WIDTH)
#define IC_IMAGE_HEIGHT          (CAM_IMG_FULL_HEIGHT)

static I2CM_Device _i2c0dev;
static I2CM_Device _i2c1dev;
static I2CM_Device _i2c2dev;

static I2CM_Device *i2c0Dev = &_i2c0dev;
static I2CM_Device *i2c1Dev = &_i2c1dev;
static I2CM_Device *i2c2Dev = &_i2c2dev;

static hdmiCfg_t hdmiCfg = {
    ADV7513_1080P60,
    NULL, //I2C handle, set runtime
    &lcdSpec1080p60
};

static sendOutInitCfg_t sendCfg = {
    &hdmiCfg,
    NULL,
    NULL
};

typedef struct {
    unsigned int ref_cnt;
    osal_mutex *lock;
    icCtrl  *ctrl;
    pool_t *mipi_header_pool;
    struct {
        mipi_tx_thread_client_t *client[FRAME_DATA_TYPE_MAX][FRAME_T_FORMAT_MAX];
        pthread_mutex_t lock;
        pthread_cond_t cond;
        int wait_to_send_in_mipi_tx;
    } per_instance[INSTANCES_COUNT_MAX];
} g_ipipe_ctx_t;

extern guzzi_camera3_enum_z_custom_usecase_selection_t get_cust_usecase(int cam_id);

void los_ConfigureSource(
        int srcIdx,
        icSourceConfig *sconf,
        int pipeId
    );
void los_SetupSource(
        int srcIdx,
        icSourceSetup *sconf
    );
void los_SetupSourceCommit(void);
void inc_cam_stats_ready(
        void *p_prv,
        unsigned int seqNo,
        void *p_cfg_prv
    );
void inc_cam_capture_ready(
        void *p_prv,
        unsigned int seqNo,
        void *p_cfg_prv
    );
void inc_cam_ipipe_cfg_ready(
        void *p_prv,
        unsigned int seqNo,
        void *p_cfg_prv
    );
void inc_cam_ipipe_buff_locked(
        void *p_prv,
        void *userData,
        unsigned int sourceInstance,
        void *buffZsl,
        icTimestamp ts,
        unsigned int seqNo
    );
void inc_cam_frame_start(
        void *p_prv,
        uint32_t sourceInstance,
        uint32_t seqNo,
        icTimestamp ts,
        void *userData
    );
void inc_cam_frame_line_reached(
        void *p_prv,
        uint32_t sourceInstance,
        uint32_t seqNo,
        icTimestamp ts,
        void *userData
    );
void inc_cam_frame_end(
        void *p_prv,
        uint32_t sourceInstance,
        uint32_t seqNo,
        icTimestamp ts,
        void *userData
    );
void los_dataWasSent(
        void *dataBufStruct
    );

void inc_cam_terminate_fr(
        void *p_prv,
        uint32_t sourceInstance,
        void *userData
    );

static g_ipipe_ctx_t gctx;

sem_t semWaitForLrtReady;
sem_t semWaitForSourceCommit;
sem_t semWaitForSourceReady;
sem_t semWaitForSourceStoped;

static pthread_t eventThread;

icSourceSetup srcSet = {
    IC_IMAGE_WIDTH, IC_IMAGE_HEIGHT, 10, (IC_IMAGE_WIDTH * IC_IMAGE_HEIGHT), 1, 1, 1, 1, 0
};

#define INSTANCES   INSTANCES_COUNT_MAX

#define SRC_BUFS    3
#define ISP_BUFS    4
#define STILL_BUFS  1

#define DDR_BUFFER_ALOCATED_MEM_SIZE \
( \
      INSTANCES \
    * ( \
         ((10 * IC_IMAGE_WIDTH * IC_IMAGE_HEIGHT / 8) * SRC_BUFS) \
       + ((IC_IMAGE_WIDTH * IC_IMAGE_HEIGHT * 3 / 2) * ISP_BUFS) \
       + ((IC_IMAGE_WIDTH * IC_IMAGE_HEIGHT * 3 / 2) * STILL_BUFS) \
       + 1024 \
      ) \
    + ((PANO_WIDTH_VIDEO * PANO_HEIGHT_VIDEO * 2) * ISP_BUFS) \
    + ((PANO_WIDTH_STILL * PANO_HEIGHT_STILL * 2) * STILL_BUFS) \
)

static uint8_t ddrStaticAlocatedMemory[DDR_BUFFER_ALOCATED_MEM_SIZE] __attribute__((
        section(".ddr.bss")
    ));

/*
 * ****************************************************************************
 * ** MIPI Tx Settings ********************************************************
 * ****************************************************************************
 */
#define MIPI_TX_WIDTH_FROM_NV12_WIDTH(NV12_WIDTH)       ((NV12_WIDTH) / 2)
#define MIPI_TX_HEIGHT_FROM_NV12_HEIGHT(NV12_WIDTH)     (3 * (NV12_WIDTH) / 2)
#define MIPI_TX_LINESZ_FROM_NV12_LINESZ(NV12_WIDTH)     (NV12_WIDTH)

#define MIPI_TX_WIDTH_FROM_YUV422_WIDTH(YUV422_WIDTH)   ((YUV422_WIDTH) / 2)
#define MIPI_TX_HEIGHT_FROM_YUV422_HEIGHT(YUV422_HEIGHT)(2 * (YUV422_HEIGHT))
#define MIPI_TX_LINESZ_FROM_YUV422_LINESZ(YUV422_LINESZ)(YUV422_LINESZ)

#define MIPI_TX_WIDTH_FROM_RAW_WIDTH(RAW_WIDTH)         ((RAW_WIDTH) / 2)
#define MIPI_TX_HEIGHT_FROM_RAW_HEIGHT(RAW_HEIGHT)      (5 * (RAW_HEIGHT) / 4)
#define MIPI_TX_LINESZ_FROM_RAW_LINESZ(RAW_LINESZ)      (RAW_LINESZ)

#if defined(LENOVO360CAM_BOARD) || defined(LENOVO360CAMV2_BOARD) || defined(LENOVO360CAMV3_BOARD)
#define MIPI_TX_NUM_LANES       4
#define MIPI_TX_CLOCK           300
#else
#define MIPI_TX_NUM_LANES       2
#define MIPI_TX_CLOCK           400
#endif
#define MIPI_TX_USE_IRQ         1

// Never send Meta data
#define DO_NOT_SEND_METADATA

// Do not send isp dump data over Mipi
//#define SEND_ISP_DATA_OVER_MIPI

// HDMI Power-Down Control
#define GPIO__HDMI_PD           28

// Skip a number of frames on Camera start
#define FRAMES_TO_SKIP  (8)

static mipi_tx_board_params_t _mipi_tx_board_params = {
    .mipi_controller = MIPI_CTRL_5,
    .mipi_ref_clock_kHz = 12000,
};
mipi_tx_board_params_t *mipi_tx_board_params = &_mipi_tx_board_params;

static mipi_tx_thread_create_t mipi_params = {
    .prio       = 200,
    .stack_size = 4*1024,
    .num_lanes  = MIPI_TX_NUM_LANES,
    .mipi_clock = MIPI_TX_CLOCK,
    .use_irq    = MIPI_TX_USE_IRQ
};

/***********  Preview TX settings  ***********/
#define PREVIEW_WIDTH           PANO_WIDTH //IC_IMAGE_WIDTH
#define PREVIEW_HEIGHT          PANO_HEIGHT //IC_IMAGE_HEIGHT

#if (defined STREAM_ON_MIPI && (defined(LENOVO360CAM_BOARD) || defined(LENOVO360CAMV2_BOARD) || defined(LENOVO360CAMV3_BOARD)))
#define PREVIEW_DATA_TYPE       MIPI_DT_CSI2_YUV_422_8BIT
#else
#define PREVIEW_DATA_TYPE       MIPI_DT_CSI2_USER_2
#endif

#define PREVIEW_PRIORITY        10 /* Lower number higher priority */
#define PREVIEW_CHUNKS_NUM      1
#define PREVIEW_HEADER_HEIGHT   2

#define PREVIEW_WIDTH_MIPI      MIPI_TX_WIDTH_FROM_YUV422_WIDTH(PREVIEW_WIDTH)
#define PREVIEW_HEIGHT_MIPI	    MIPI_TX_ROUND_UP(MIPI_TX_HEIGHT_FROM_YUV422_HEIGHT(PREVIEW_HEIGHT),PREVIEW_CHUNKS_NUM)
#define PREVIEW_LINESZ_MIPI     MIPI_TX_LINESZ_FROM_YUV422_LINESZ(PREVIEW_WIDTH)

#define PREVIEW_HEADER_STORE_SIZE (PREVIEW_LINESZ_MIPI*PREVIEW_HEADER_HEIGHT)
#define PREVIEW_HEADERS_COUNT     (INSTANCES_COUNT_MAX * 2)

static mipi_tx_characteristics_t mipi_tx_prv_characteristics = {
    .priority           = PREVIEW_PRIORITY,
    .data_type          = PREVIEW_DATA_TYPE,
    .width              = PREVIEW_WIDTH_MIPI,
    .height_1           = 2 * PREVIEW_HEIGHT_MIPI,
    .height_2           = 0, /* plane 2 is not used */
    .height_3           = 0, /* plane 3 is not used */
    .line_size          = PREVIEW_LINESZ_MIPI,
    .header_height      = PREVIEW_HEADER_HEIGHT,
    .header_data_type   = PREVIEW_DATA_TYPE,
    .num_chunks         = PREVIEW_CHUNKS_NUM,
};

/***********  Still TX settings  ***********/
#define STILL_WIDTH           PANO_WIDTH_STILL
#define STILL_HEIGHT          PANO_HEIGHT_STILL
#if (defined STREAM_ON_MIPI && (defined(LENOVO360CAM_BOARD) || defined(LENOVO360CAMV2_BOARD) || defined(LENOVO360CAMV3_BOARD)))
#define STILL_DATA_TYPE       (PREVIEW_DATA_TYPE) // Same type as preview data
#else
#define STILL_DATA_TYPE       MIPI_DT_CSI2_USER_1 // MIPI_DT_CSI2_USER_2
#endif
#define STILL_PRIORITY        10 /* Lower number higher priority */
#define STILL_CHUNKS_NUM      1
#define STILL_HEADER_HEIGHT   2

#define STILL_WIDTH_MIPI        MIPI_TX_WIDTH_FROM_YUV422_WIDTH(STILL_WIDTH)
#define STILL_HEIGHT_MIPI       MIPI_TX_ROUND_UP(MIPI_TX_HEIGHT_FROM_YUV422_HEIGHT(STILL_HEIGHT),STILL_CHUNKS_NUM)
#define STILL_LINESZ_MIPI       MIPI_TX_LINESZ_FROM_YUV422_LINESZ(STILL_WIDTH)

#define STILL_HEADER_STORE_SIZE (STILL_LINESZ_MIPI*STILL_HEADER_HEIGHT)
#define STILL_HEADERS_COUNT       (INSTANCES_COUNT_MAX * 1)

static mipi_tx_characteristics_t mipi_tx_still_characteristics = {
    .priority           = STILL_PRIORITY,
    .data_type          = STILL_DATA_TYPE,
    .width              = STILL_WIDTH_MIPI,
    .height_1           = 2 * STILL_HEIGHT_MIPI,
    .height_2           = 0, /* plane 2 is not used */
    .height_3           = 0, /* plane 3 is not used */
    .line_size          = STILL_LINESZ_MIPI,
    .header_height      = STILL_HEADER_HEIGHT,
    .header_data_type   = STILL_DATA_TYPE,
    .num_chunks         = STILL_CHUNKS_NUM,
};

/***********  RAW TX settings  ***********/
#define RAW_DATA_TYPE       MIPI_DT_CSI2_USER_1 // MIPI_DT_CSI2_USER_1
#define RAW_PRIORITY        10 /* Lower number higher priority */
#define RAW_CHUNKS_NUM      1
#define RAW_HEADER_HEIGHT   2

#define RAW_HEADERS_COUNT       (INSTANCES_COUNT_MAX * 1)

static mipi_tx_characteristics_t mipi_tx_raw_characteristics = {
    .priority           = RAW_PRIORITY,
    .data_type          = RAW_DATA_TYPE,
    .width              = 0, //Set per client.
    .height_1           = 0,
    .height_2           = 0, /* plane 2 is not used */
    .height_3           = 0, /* plane 3 is not used */
    .line_size          = 0,
    .header_height      = RAW_HEADER_HEIGHT,
    .header_data_type   = RAW_DATA_TYPE,
    .num_chunks         = RAW_CHUNKS_NUM,
};

/***********  Metadata TX settings  ***********/
#define METADATA_WIDTH           (1024)
#define METADATA_DATA_TYPE       (MIPI_DT_CSI2_USER_3) // MIPI_DT_CSI2_USER_1
#define METADATA_PRIORITY        (10) /* Lower number higher priority */
#define METADATA_CHUNKS_NUM      (1)
#define METADATA_HEADER_HEIGHT   (2)

#define METADATA_WIDTH_MIPI      MIPI_TX_WIDTH_FROM_YUV422_WIDTH(METADATA_WIDTH)
#define METADATA_LINESZ_MIPI     MIPI_TX_LINESZ_FROM_YUV422_LINESZ(METADATA_WIDTH)

#define METADATA_HEADERS_COUNT       (INSTANCES_COUNT_MAX * 1)

static mipi_tx_characteristics_t mipi_tx_meta_characteristics = {
    .priority           = METADATA_PRIORITY,
    .data_type          = METADATA_DATA_TYPE,
    .width              = METADATA_WIDTH_MIPI,
    .height_1           = 0, /* Set by client */
    .height_2           = 0, /* plane 2 is not used */
    .height_3           = 0, /* plane 3 is not used */
    .line_size          = METADATA_WIDTH,
    .header_height      = METADATA_HEADER_HEIGHT,
    .header_data_type   = METADATA_DATA_TYPE,
    .num_chunks         = METADATA_CHUNKS_NUM,
};

/***********  IQ Debug TX settings  ***********/
#define IQ_DEBUG_WIDTH           (1024)
#define IQ_DEBUG_DATA_TYPE       (MIPI_DT_CSI2_USER_4)
#define IQ_DEBUG_PRIORITY        (10) /* Lower number higher priority */
#define IQ_DEBUG_CHUNKS_NUM      (1)
#define IQ_DEBUG_HEADER_HEIGHT   (2)

#define IQ_DEBUG_WIDTH_MIPI MIPI_TX_WIDTH_FROM_NV12_WIDTH(IQ_DEBUG_WIDTH)
#define IQ_DEBUG_LINESZ_MIPI MIPI_TX_LINESZ_FROM_NV12_LINESZ(IQ_DEBUG_WIDTH)

#define IQ_DEBUG_HEADERS_COUNT       (INSTANCES_COUNT_MAX * 1)

static mipi_tx_characteristics_t mipi_tx_iq_debug_characteristics = {
    .priority           = IQ_DEBUG_PRIORITY,
    .data_type          = IQ_DEBUG_DATA_TYPE,
    .width              = IQ_DEBUG_WIDTH_MIPI,
    .height_1           = 0, /* Set by client */
    .height_2           = 0, /* plane 2 is not used */
    .height_3           = 0, /* plane 3 is not used */
    .line_size          = IQ_DEBUG_LINESZ_MIPI,
    .header_height      = IQ_DEBUG_HEADER_HEIGHT,
    .header_data_type   = IQ_DEBUG_DATA_TYPE,
    .num_chunks         = IQ_DEBUG_CHUNKS_NUM,
};

/***********  ALL TX settings  ***********/

//TODO: Remove this, init header pools per client?
#define MAX_LINESIZE_MIPI      (4208 * 2)
#define MAX_HEADER_HEIGHT      (2)
#define MAX_HEADER_STORE_SIZE  (MAX_LINESIZE_MIPI * MAX_HEADER_HEIGHT)

#define TOTAL_HEADERS_COUNT \
( \
      PREVIEW_HEADERS_COUNT \
    + STILL_HEADERS_COUNT \
    + RAW_HEADERS_COUNT \
    + METADATA_HEADERS_COUNT \
    + IQ_DEBUG_HEADERS_COUNT \
)

typedef struct {
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t header_height;
    uint32_t slice_total_number;
    uint32_t pxl_size_nom;
    uint32_t pxl_size_denom;
} client_data_lut_t;

client_data_lut_t tx_lut[INSTANCES_COUNT_MAX][FRAME_DATA_TYPE_MAX][FRAME_T_FORMAT_MAX];

// Protecttion flag for the Still capture
uint32_t capture_busy = 0;

static void * update_frame_header(void *header, uint32_t outputId, uint32_t frmType, uint32_t frmFmt)
{
    mipi_tx_header_still_image_t *mipi_tx_header;
    client_tx_frame_header_t *h;

    mipi_tx_header = header;
    h = (client_tx_frame_header_t *)mipi_tx_header->client_data;

    h->frame_type               = frmType;          // FRAME_DATA_TYPE_PREVIEW;
    h->frame_format             = frmFmt;           //FRAME_T_FORMAT_422I;
    h->frame_width              = tx_lut[outputId][frmType][frmFmt].frame_width;  // PREVIEW_WIDTH;
    h->frame_height             = tx_lut[outputId][frmType][frmFmt].frame_height; // PREVIEW_HEIGHT;
    h->frame_time_stamp_hi      = 0;
    h->frame_time_stamp_lo      = 0;
    h->frame_proc_time_stamp_hi = 0;
    h->frame_proc_time_stamp_lo = 0;
    h->frame_idx_req_hal        = 0;
    h->frame_idx_req_app        = 0;
    h->frame_idx_mipi_rx        = 0;
    h->frame_idx_process        = 0;
    h->header_height            = tx_lut[outputId][frmType][frmFmt].header_height; // PREVIEW_HEADER_HEIGHT;
    h->slice_data_type          = 0;
    h->slice_y_offset           = 0;
    h->slice_y_size             = 0;
    h->slice_uv_offset          = 0;
    h->slice_uv_size            = 0;
    h->slice_total_number       = tx_lut[outputId][frmType][frmFmt].slice_total_number; // PREVIEW_CHUNKS_NUM;
    h->slice_last_flag          = 0;
    h->debug_data_enable        = 0;
    h->camera_id                = outputId;
    h->buff_width               = h->frame_width;
    h->buff_height              = h->frame_height;
    h->buff_stride              = h->buff_width;
    h->buff_pxl_size_nom        = tx_lut[outputId][frmType][frmFmt].pxl_size_nom;
    h->buff_pxl_size_denom      = tx_lut[outputId][frmType][frmFmt].pxl_size_denom;
    h->check_sum                = mipi_tx_header_calc_sum(h);
	static unsigned long count = 1 ;
	static int tmp = 0;
	if(count == 0)
		tmp ++;
	printf("update_frame_header    %lu   %d\n" , count++ ,tmp);
    return header;
}

/*
 * ****************************************************************************
 * ** Misc ********************************************************************
 * ****************************************************************************
 */

/*
 * Fps meter
 */
float fpsCams[IPIPE_MAX_OUTPUTS_ALlOWED] = {0};
uint32_t frmTotalCount[IPIPE_MAX_OUTPUTS_ALlOWED] = {0};

static void updateFps(uint32_t outputId)
{
    // Times are in uS
    const uint64_t cMeasureInterval = 1 * 1000 * 1000; // Period to refresh fps value (3s)
    static uint64_t frmCntr[IPIPE_MAX_OUTPUTS_ALlOWED];
    static uint64_t measureStartTime[IPIPE_MAX_OUTPUTS_ALlOWED];
    uint64_t currTime;
    struct timespec currTs;

    frmCntr[outputId]++;
    frmTotalCount[outputId]++;

    clock_gettime(CLOCK_REALTIME, &currTs);

    currTime = (uint64_t)(currTs.tv_nsec / 1000 + currTs.tv_sec * 1000000);

    if (!measureStartTime[outputId])
        measureStartTime[outputId] = currTime;

    if ((currTime - measureStartTime[outputId]) < cMeasureInterval)
        return;
    else {
        fpsCams[outputId] = frmCntr[outputId] / ((currTime - measureStartTime[outputId])/ 1000000.0f);
        measureStartTime[outputId] = currTime;
        frmCntr[outputId] = 0;
    }
}

void setLcdToMipi(void)
{
    SET_REG_WORD(MSS_MIPI_CFG_ADR, D_DRV_MSS_MIPI_CFG_C5_TX | D_DRV_MSS_MIPI_CFG_EN_5);
    SET_REG_WORD(MSS_LCD_MIPI_CFG_ADR, D_DRV_MSS_LCD_TO_MIPI_TX5);
    SET_REG_WORD(MSS_GPIO_CFG_ADR, D_DRV_MSS_LCD_TO_MIPI);
}

void free_header_and_return_frame(
        g_ipipe_ctx_t *ctx,
        mipi_tx_header_t *header,
        void *frame
    )
{
    client_tx_frame_header_t *h;

    h = (client_tx_frame_header_t *)(header->client_data);

    los_dataWasSent(frame);

    pthread_mutex_lock(&ctx->per_instance[h->camera_id].lock);
    ctx->per_instance[h->camera_id].wait_to_send_in_mipi_tx--;
    pthread_cond_broadcast(&ctx->per_instance[h->camera_id].cond);
    pthread_mutex_unlock(&ctx->per_instance[h->camera_id].lock);

    if ((FRAME_DATA_TYPE_STILL == h->frame_type) &&
        (0 == ctx->per_instance[h->camera_id].wait_to_send_in_mipi_tx)) {
        // The still frame was sent
        capture_busy = 0;
    }

    osal_free(header);
}

static void mipi_tx_sent_callback(
        mipi_tx_thread_client_t *client,
        void *clinet_prv,
        mipi_tx_header_t *header,
        void *p1,
        void *p2,
        void *p3
    )
{
    UNUSED(client);
    UNUSED(p1);
    UNUSED(p2);
    free_header_and_return_frame(clinet_prv, header, p3);
}

static void mipi_meta_tx_sent_callback(
        mipi_tx_thread_client_t *client,
        void *clinet_prv,
        mipi_tx_header_t *header,
        void *p1,
        void *p2,
        void *p3
    ){
    UNUSED(clinet_prv);
    UNUSED(p2);

    client_tx_frame_header_t *h;

    h = (client_tx_frame_header_t *)(header->client_data);

    icMipiTxClientDataSent(h->camera_id, client, h->frame_width * h->frame_height, p1, p3);

    pthread_mutex_lock(&gctx.per_instance[h->camera_id].lock);
    gctx.per_instance[h->camera_id].wait_to_send_in_mipi_tx--;
    pthread_cond_broadcast(&gctx.per_instance[h->camera_id].cond);
    pthread_mutex_unlock(&gctx.per_instance[h->camera_id].lock);
    osal_free(header);
}

static void mipi_iq_debug_tx_sent_callback(
        mipi_tx_thread_client_t *client,
        void *clinet_prv,
        mipi_tx_header_t *header,
        void *p1,
        void *p2,
        void *p3
    )
{
    UNUSED(client);
    UNUSED(clinet_prv);
    UNUSED(p1);
    UNUSED(p2);
    UNUSED(p3);
    osal_free(header);
}

/*
 * ****************************************************************************
 * ** Should run at a relatively high priority so that the event queue doesn't
 * ** fill up (if it did, we would drop events).
 * ****************************************************************************
 */
static void * eventLoop(void *vCtrl)
{
    g_ipipe_ctx_t *ctx;
    icCtrl *ctrl;
    icEvent ev;
    unsigned int evno;
    static uint32_t skip_frames = FRAMES_TO_SKIP;

    ctx = vCtrl;
    ctrl = ctx->ctrl;

    /*
     * This thread runs until it is cancelled (pthread_cond_wait() is a
     * cancellation point).
     */
    for (;;) {
        if (!icGetEvent(ctrl, &ev)) {
            evno = ev.ctrl & IC_EVENT_CTRL_TYPE_MASK;

            switch (evno) {
            case IC_EVENT_TYPE_LEON_RT_READY:
                /* TODO: PROFILE_ADD */
                assert(sem_post(&semWaitForLrtReady) != -1);
                break;
            case IC_EVENT_TYPE_SETUP_SOURCES_RESULT:
                /* TODO: PROFILE_ADD */
                assert(sem_post(&semWaitForSourceCommit) != -1);
                break;
            case IC_EVENT_TYPE_SOURCE_READY:
                /* TODO: PROFILE_ADD */
                assert(sem_post(&semWaitForSourceReady) != -1);
                break;
            case IC_EVENT_TYPE_READOUT_START:
                if (ev.u.lineEvent.userData != NULL) {
                    PROFILE_ADD(
                            PROFILE_ID_LRT_START_FRAME,
                            ev.u.lineEvent.userData,
                            ev.u.lineEvent.sourceInstance
                        );
                    inc_cam_frame_start(
                            NULL,
                            ev.u.lineEvent.sourceInstance,
                            ev.u.lineEvent.seqNo,
                            ev.u.lineEvent.ts,
                            ev.u.lineEvent.userData
                        );
                    inc_cam_ipipe_cfg_ready(
                            NULL,
                            ev.u.ispEvent.seqNo,
                            ev.u.ispEvent.userData
                        );
                }
                break;
            case IC_EVENT_TYPE_LINE_REACHED:
                if (ev.u.lineEvent.userData != NULL) {
                    PROFILE_ADD(
                            PROFILE_ID_LRT_LINE_REACHED,
                            ev.u.lineEvent.userData,
                            ev.u.lineEvent.sourceInstance
                        );
                    #if 0 /* TODO: IPIPE2 not implelmented */
                    inc_cam_frame_line_reached(
                            NULL,
                            ev.u.lineEvent.sourceInstance,
                            ev.u.lineEvent.seqNo,
                            ev.u.lineEvent.ts,
                            ev.u.lineEvent.userData
                        );
                    #endif
                }
                break;
            case IC_EVENT_TYPE_READOUT_END:
                if (ev.u.lineEvent.userData != NULL) {
                    PROFILE_ADD(
                            PROFILE_ID_LRT_END_FRAME,
                            ev.u.lineEvent.userData,
                            ev.u.lineEvent.sourceInstance
                        );
                    inc_cam_frame_end(
                            NULL,
                            ev.u.lineEvent.sourceInstance,
                            ev.u.lineEvent.seqNo,
                            ev.u.lineEvent.ts,
                            ev.u.lineEvent.userData
                        );
                }
                break;
            case IC_EVENT_TYPE_ISP_START:
                if (ev.u.ispEvent.userData != NULL) {
                    PROFILE_ADD(
                            PROFILE_ID_LRT_ISP_START,
                            ev.u.ispEvent.userData,
                            ev.u.ispEvent.ispInstance
                        );
                    #if 0 /* TODO: IPIPE2 not implelmented */
                    inc_cam_ipipe_cfg_ready(
                            NULL,
                            ev.u.ispEvent.seqNo,
                            ev.u.ispEvent.userData
                        );
                    #endif
                } else {
                    mmsdbg(
                            DL_ERROR,
                            "IC_EVENT_TYPE_ISP_START with userdata == NULL"
                        );
                }
                break;
            case IC_EVENT_TYPE_ISP_END:
                if (NULL != ev.u.ispEvent.userData ) {
                    PROFILE_ADD(
                            PROFILE_ID_LRT_ISP_END,
                            ev.u.ispEvent.userData,
                            ev.u.ispEvent.ispInstance
                        );
                    inc_cam_stats_ready(
                            NULL,
                            ev.u.ispEvent.seqNo,
                            ev.u.ispEvent.userData
                        );
                    #if 0 /* TODO: IPIPE2 not implelmented */
                    inc_cam_capture_ready(
                            NULL,
                            ev.u.ispEvent.seqNo,
                            ev.u.ispEvent.userData
                        );
                    #endif
                } else {
                    mmsdbg(
                            DL_ERROR,
                            "IC_EVENT_TYPE_ISP_END with userdata == NULL"
                        );
                }
                break;
            case IC_EVENT_TYPE_ZSL_LOCKED:
                PROFILE_ADD(
                        PROFILE_ID_LRT_ZSL_LOCKED,
                        ev.u.buffLockedZSL.userData,
                        ev.u.buffLockedZSL.sourceInstance
                    );
                inc_cam_ipipe_buff_locked(
                        NULL,
                        ev.u.buffLockedZSL.userData,
                        ev.u.buffLockedZSL.sourceInstance,
                        ev.u.buffLockedZSL.buffZsl,
                        ev.u.buffLockedZSL.buffZsl->timestamp[0], // time stamp when zsl was arrived in memory
                        //ev.u.buffLockedZSL.buffZsl->timestamp[ev.u.buffLockedZSL.buffZsl->timestampNr-1] // time stamp when frame was processed
                        ev.u.buffLockedZSL.buffZsl->seqNo
                    );

                break;
            case IC_EVENT_TYPE_SOURCE_STOPPED:
                /* TODO: PROFILE_ADD */
                assert(sem_post(&semWaitForSourceStoped) != -1);
                break;
            case IC_EVENT_TYPE_SEND_OUTPUT_DATA: {
                FrameT *frame;
                uint32_t outId;
                uint32_t frmType;
                uint32_t frmFmt;

                frame   = ev.u.sentOutput.dataBufStruct;
                // it is a app specific output type. In this case as not directly dependent output id by source is not supported,
                // will be made a hack, depth output will be register as  output id 2, FRAME_DATA_TYPE_STILL, FRAME_T_FORMAT_YUV420
                outId   = ev.u.sentOutput.outputId;
                frmFmt = frame->type;
                frmType = FRAME_DATA_TYPE_PREVIEW;
                if(outId)
                    frmType = FRAME_DATA_TYPE_STILL;

                PROFILE_ADD(
                        PROFILE_ID_LRT_SEND_DATA,
                        frame->appSpecificData,
                        outId
                    );

                assert(outId<INSTANCES_COUNT_MAX);
                assert(frmType<FRAME_DATA_TYPE_MAX);
                assert(frmFmt<FRAME_T_FORMAT_MAX);

                if (!skip_frames) {
                    updateFps(ev.u.sentOutput.outputId);

#ifndef STREAM_ON_MIPI
                    sendOutSend(frame, outId, frmType, los_dataWasSent);

                    if (FRAME_DATA_TYPE_STILL == frmType) {
                        // The still frame was sent
                        capture_busy = 0;
                    }
#else
                    if (NULL == ctx->per_instance[outId].client[frmType][frmFmt])
                    {
                        mmsdbg(DL_ERROR, "No TX client for: outId %lu frmType %lu frmFmt %lu", outId, frmType, frmFmt);
                        break;
                    }

                    pthread_mutex_lock(&ctx->per_instance[outId].lock);
                    ctx->per_instance[outId].wait_to_send_in_mipi_tx++;
                    pthread_mutex_unlock(&ctx->per_instance[outId].lock);  

					extern char txBuff[4];
					//txBuff[0] = 0 ;
					txBuff[1] = 1 ;
					txBuff[2] = 2 ;
					//txBuff[3] = 3 ;
					memcpy(frame->fbPtr[0] ,txBuff, sizeof(txBuff));
				 // memset(frame->fbPtr[0]+sizeof(txBuff),0,1920*100);
				 //rintf("bufsize:%d\n" , );
                    assert(!mipi_tx_thread_send(
                            ctx->per_instance[outId].client[frmType][frmFmt],
                            update_frame_header(
                                    pool_alloc(ctx->mipi_header_pool),
                                    outId,
                                    frmType,
                                    frmFmt
                                ),
                            frame->fbPtr[0],
                            frame->fbPtr[1],
                            frame
                        ));
#endif
                } else {
                    // skip a frame
                    los_dataWasSent(frame);
                    skip_frames--;
                }
                }
                break;
            case IC_EVENT_TYPE_ERROR:
                PROFILE_ADD(
                        PROFILE_ID_LRT_ERROR,
                        ev.u.error.userData,
                        ev.u.error.sourceInstance
                    );
                if ((ev.u.error.userData) && (ev.u.error.errorNo != IC_ERROR_NONE)) {
                    inc_cam_terminate_fr(
                            NULL,
                            ev.u.error.sourceInstance,
                            ev.u.error.userData
                        );
                }
                break;
            case IC_EVENT_TYPE_TORN_DOWN:
                /* TODO: PROFILE_ADD */
                ipipeTogglePowerState(IdlePw); /// All cameras off so go to idle
                // Reset the frame skip counter
                skip_frames = FRAMES_TO_SKIP;
                return NULL;
            default:
                mmsdbg(DL_ERROR, "Unknown evt: %d", evno);
            }
        } else {
            /*
             * no activity in last period, lrt crash
             * or lrt is started, but no camera connected
             * in this case cut down lrt
             */
            mmsdbg(DL_ERROR, "Error X");
        }
    }

    return NULL;
}

static void deregister_tx_clients_for_camera (int inst){
    uint32_t frm_type;
    uint32_t fmt;

    for(frm_type = 0; frm_type < FRAME_DATA_TYPE_MAX; ++frm_type){
        for (fmt = 0; fmt < FRAME_T_FORMAT_MAX; ++fmt){
            mipi_tx_thread_client_unregister(&gctx.per_instance[inst].client[frm_type][fmt]);
        }
    }
}

static void invalidate_tx_clients (void)
{
    uint32_t inst;

    for (inst = 0; inst < INSTANCES_COUNT_MAX; ++inst) {
        deregister_tx_clients_for_camera(inst);
    }
}

static void init_tx_clients_for_all_cameras (mipi_tx_thread_client_t *p_client, uint32_t frm_type, uint32_t fmt)
{
uint32_t inst;

    for (inst = 0; inst < INSTANCES_COUNT_MAX; ++inst) {
        gctx.per_instance[inst].client[frm_type][fmt] = p_client;
    }
}

static void init_preview_client_for_source(int srcIdx, int pipeId, uint32_t width, uint32_t height){
    mipi_tx_thread_client_t *p_client;

    // Check if preview is downscaled in the preview pipe
    if (pipeId == 1){
        width = width/2;
        height = height/2;
    }

    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_YUV420].frame_width = width;
    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_YUV420].frame_height = height;
    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_YUV420].header_height = PREVIEW_HEADER_HEIGHT;
    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_YUV420].slice_total_number = PREVIEW_CHUNKS_NUM;
    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_YUV420].pxl_size_nom = 3;
    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_YUV420].pxl_size_denom = 2;

    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_422I].frame_width = width;
    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_422I].frame_height = height;
    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_422I].header_height = PREVIEW_HEADER_HEIGHT;
    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_422I].slice_total_number = PREVIEW_CHUNKS_NUM;
    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_422I].pxl_size_nom = 2;
    tx_lut[srcIdx][FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_422I].pxl_size_denom = 1;

    mipi_tx_prv_characteristics.width       = MIPI_TX_WIDTH_FROM_YUV422_WIDTH(width);
    mipi_tx_prv_characteristics.height_1    = MIPI_TX_ROUND_UP(height, PREVIEW_CHUNKS_NUM);
    mipi_tx_prv_characteristics.height_2    = 0;
    mipi_tx_prv_characteristics.height_3    = 0;
    mipi_tx_prv_characteristics.line_size   = MIPI_TX_LINESZ_FROM_YUV422_LINESZ(width);

    assert((p_client=mipi_tx_thread_client_register(
        &mipi_tx_prv_characteristics,
        &gctx,
        mipi_tx_sent_callback
    )));
    gctx.per_instance[srcIdx].client[FRAME_DATA_TYPE_PREVIEW][FRAME_T_FORMAT_422I] = p_client;
}

static void init_capture_client_for_source(int srcIdx, int pipeId, uint32_t width, uint32_t height){
    mipi_tx_thread_client_t *p_client;

    UNUSED(pipeId);

    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_YUV420].frame_width = width;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_YUV420].frame_height = height;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_YUV420].header_height = STILL_HEADER_HEIGHT;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_YUV420].slice_total_number = STILL_CHUNKS_NUM;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_YUV420].pxl_size_nom = 3;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_YUV420].pxl_size_denom = 2;

    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_422I].frame_width = width;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_422I].frame_height = height;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_422I].header_height = STILL_HEADER_HEIGHT;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_422I].slice_total_number = STILL_CHUNKS_NUM;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_422I].pxl_size_nom = 2;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_422I].pxl_size_denom = 1;

    mipi_tx_still_characteristics.width       = MIPI_TX_WIDTH_FROM_YUV422_WIDTH(width);
    mipi_tx_still_characteristics.height_1    = MIPI_TX_ROUND_UP(height, STILL_CHUNKS_NUM);
    mipi_tx_still_characteristics.height_2    = 0;
    mipi_tx_still_characteristics.height_3    = 0;
    mipi_tx_still_characteristics.line_size   = MIPI_TX_LINESZ_FROM_YUV422_LINESZ(width);

    assert((p_client=mipi_tx_thread_client_register(
        &mipi_tx_still_characteristics,
        &gctx,
        mipi_tx_sent_callback
    )));
    gctx.per_instance[srcIdx].client[FRAME_DATA_TYPE_STILL][FRAME_T_FORMAT_422I] = p_client;
}

// Assumes data type of the client will be MOVIDIUS RAW
static void init_raw_client_for_source(int srcIdx, int pipeId, uint32_t width, uint32_t height)
{
    mipi_tx_thread_client_t *p_client;
    uint32_t width_bytes;

    UNUSED(pipeId);

    width_bytes = (width * 5) / 4;

    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL_RAW][FRAME_T_FORMAT_RAW_10_PACK].frame_width = width;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL_RAW][FRAME_T_FORMAT_RAW_10_PACK].frame_height = height;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL_RAW][FRAME_T_FORMAT_RAW_10_PACK].header_height = RAW_HEADER_HEIGHT;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL_RAW][FRAME_T_FORMAT_RAW_10_PACK].slice_total_number = RAW_CHUNKS_NUM;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL_RAW][FRAME_T_FORMAT_RAW_10_PACK].pxl_size_nom = 5;
    tx_lut[srcIdx][FRAME_DATA_TYPE_STILL_RAW][FRAME_T_FORMAT_RAW_10_PACK].pxl_size_denom = 4;

    mipi_tx_raw_characteristics.width         = MIPI_TX_WIDTH_FROM_RAW_WIDTH(width_bytes);
    mipi_tx_raw_characteristics.height_1      = MIPI_TX_ROUND_UP(height, RAW_CHUNKS_NUM);
    mipi_tx_raw_characteristics.height_2      = 0;
    mipi_tx_raw_characteristics.height_3      = 0;
    mipi_tx_raw_characteristics.line_size     = MIPI_TX_LINESZ_FROM_RAW_LINESZ(width_bytes);

    assert((p_client=mipi_tx_thread_client_register(
        &mipi_tx_raw_characteristics,
        &gctx,
        mipi_tx_sent_callback
    )));
    gctx.per_instance[srcIdx].client[FRAME_DATA_TYPE_STILL_RAW][FRAME_T_FORMAT_RAW_10_PACK] = p_client;
}

static void* init_meta_client_for_source(int srcIdx, uint32_t dataSizeMax){
    mipi_tx_thread_client_t *p_client;
    uint32_t height = ((dataSizeMax + METADATA_LINESZ_MIPI - 1) / METADATA_LINESZ_MIPI);

    tx_lut[srcIdx][FRAME_DATA_TYPE_METADATA_PRV][FRAME_T_FORMAT_BINARY].frame_width = METADATA_WIDTH;
    tx_lut[srcIdx][FRAME_DATA_TYPE_METADATA_PRV][FRAME_T_FORMAT_BINARY].frame_height = height;
    tx_lut[srcIdx][FRAME_DATA_TYPE_METADATA_PRV][FRAME_T_FORMAT_BINARY].header_height = METADATA_HEADER_HEIGHT;
    tx_lut[srcIdx][FRAME_DATA_TYPE_METADATA_PRV][FRAME_T_FORMAT_BINARY].slice_total_number = METADATA_CHUNKS_NUM;
    tx_lut[srcIdx][FRAME_DATA_TYPE_METADATA_PRV][FRAME_T_FORMAT_BINARY].pxl_size_nom = 1;
    tx_lut[srcIdx][FRAME_DATA_TYPE_METADATA_PRV][FRAME_T_FORMAT_BINARY].pxl_size_denom = 1;

    mipi_tx_meta_characteristics.width         = METADATA_WIDTH_MIPI;
    mipi_tx_meta_characteristics.height_1      = MIPI_TX_ROUND_UP(height, METADATA_CHUNKS_NUM);
    mipi_tx_meta_characteristics.height_2      = 0;
    mipi_tx_meta_characteristics.height_3      = 0;
    mipi_tx_meta_characteristics.line_size     = METADATA_LINESZ_MIPI;

    assert((p_client=mipi_tx_thread_client_register(
        &mipi_tx_meta_characteristics,
        &gctx,
        mipi_meta_tx_sent_callback
    )));

    gctx.per_instance[srcIdx].client[FRAME_DATA_TYPE_METADATA_PRV][FRAME_T_FORMAT_BINARY] = p_client;
    return p_client;
}

static void * init_iq_debug_client_for_source(int srcIdx, uint32_t dataSizeMax)
{
    mipi_tx_thread_client_t *p_client;
    uint32_t height = ((dataSizeMax + IQ_DEBUG_LINESZ_MIPI - 1) / IQ_DEBUG_LINESZ_MIPI);

    tx_lut[srcIdx][FRAME_DATA_TYPE_IQ_DEBUG_DATA][FRAME_T_FORMAT_BINARY].frame_width = IQ_DEBUG_WIDTH;
    tx_lut[srcIdx][FRAME_DATA_TYPE_IQ_DEBUG_DATA][FRAME_T_FORMAT_BINARY].frame_height = height;
    tx_lut[srcIdx][FRAME_DATA_TYPE_IQ_DEBUG_DATA][FRAME_T_FORMAT_BINARY].header_height = IQ_DEBUG_HEADER_HEIGHT;
    tx_lut[srcIdx][FRAME_DATA_TYPE_IQ_DEBUG_DATA][FRAME_T_FORMAT_BINARY].slice_total_number = IQ_DEBUG_CHUNKS_NUM;

    mipi_tx_iq_debug_characteristics.width         = IQ_DEBUG_WIDTH_MIPI;
    mipi_tx_iq_debug_characteristics.height_1      = MIPI_TX_ROUND_UP(height, IQ_DEBUG_CHUNKS_NUM);
    mipi_tx_iq_debug_characteristics.height_2      = 0;
    mipi_tx_iq_debug_characteristics.height_3      = 0;
    mipi_tx_iq_debug_characteristics.line_size     = IQ_DEBUG_LINESZ_MIPI;

    p_client = mipi_tx_thread_client_register(
            &mipi_tx_iq_debug_characteristics,
            &gctx,
            mipi_iq_debug_tx_sent_callback
        );
    assert(p_client);

    gctx.per_instance[srcIdx].client[FRAME_DATA_TYPE_IQ_DEBUG_DATA][FRAME_T_FORMAT_BINARY] = p_client;

    return p_client;
}

static void init_tx_clients_for_source(int srcIdx, icSourceConfig *sconf, int pipeId){

    UNUSED(sconf);

    int width, height;
    width = PREVIEW_WIDTH * 2; //sconf->cropWindow.x2 - sconf->cropWindow.x1;
    height = PREVIEW_HEIGHT; //sconf->cropWindow.y2 - sconf->cropWindow.y1;

    init_preview_client_for_source(srcIdx, pipeId, width, height);
    init_capture_client_for_source(srcIdx, pipeId, PANO_WIDTH_STILL * 2 , PANO_HEIGHT_STILL);
    init_raw_client_for_source(srcIdx, pipeId, width, height);
    init_iq_debug_client_for_source(srcIdx, 500*1024);
}

/*
 * ****************************************************************************
 * ** Init hdmi function ***************************************************
 * ****************************************************************************
 */
static void initHdmi(){
    brd182InitialiseI2C(NULL,NULL,NULL,                 // Use Default I2C configuration
        &i2c0Dev,
        &i2c1Dev,
        &i2c2Dev);

    sendCfg.hdmiCfg->i2c_adv_hndl = i2c2Dev;
    sendOutInit(sendCfg);
}

void los_start(void *arg)
{
    int policy;
    struct sched_param param;

    UNUSED(arg);

    PROFILE_ADD(PROFILE_ID_LOS_START, 0, 0);

    gctx.ref_cnt++;
    if (gctx.ref_cnt == 1) {
        gctx.lock = osal_mutex_create();

        ipipeTogglePowerState(StreamPw);
        capture_busy = 0; // Clear the Still flag

        assert(sem_init(&semWaitForLrtReady, 0, 0) != -1);
        assert(sem_init(&semWaitForSourceCommit, 0, 0) != -1);
        assert(sem_init(&semWaitForSourceReady, 0, 0) != -1);
        assert(sem_init(&semWaitForSourceStoped, 0, 0) != -1);

        // All cameras are in the same usecase. Set sizes according to usecase
        if (GUZZI_CAMERA3_ENUM_Z_CUSTOM_USECASE_SELECTION_LOW_POWER_VIDEO == get_cust_usecase(0)) {
            srcSet.appSpecificInfo = SOURCE_SETUP_NO_APP_COMMAND;
            srcSet.maxWidth =  (CAM_IMG_BINN_WIDTH);
            srcSet.maxHeight = (CAM_IMG_BINN_HEIGHT);
        } else {
            srcSet.appSpecificInfo = QUARTER_MODE_FOR_VIDEO_PIPE;
            srcSet.maxWidth =  (CAM_IMG_FULL_WIDTH);
            srcSet.maxHeight = (CAM_IMG_FULL_HEIGHT);
        }

#ifdef STREAM_ON_MIPI
        gctx.mipi_header_pool = pool_create(
                "MipiTxHeaderPool",
                MAX_HEADER_STORE_SIZE,
                TOTAL_HEADERS_COUNT
            );
        assert(gctx.mipi_header_pool);
        invalidate_tx_clients ();
#endif
        gctx.ctrl = icSetup(
                1,
                (uint32_t)ddrStaticAlocatedMemory,
                sizeof (ddrStaticAlocatedMemory)
            );

        assert(pthread_create(&eventThread, NULL, eventLoop, &gctx) == 0);

        pthread_getschedparam(eventThread, &policy, &param);
        param.sched_priority = 210;
        pthread_setschedparam(eventThread, policy, &param);

        assert(sem_wait(&semWaitForLrtReady) != -1);

#ifdef STREAM_ON_MIPI
        setLcdToMipi();
        assert(!mipi_tx_thread_create(&mipi_params));
#endif

        los_SetupSource(0, &srcSet);
        los_SetupSource(1, &srcSet);
        los_SetupSource(2, &srcSet);
#ifndef STREAM_ON_MIPI
#ifdef LENOVO360CAMV2_BOARD
        // Set the HDMI_PD pin on the l360cam_v2 board
        DrvGpioSetPin(GPIO__HDMI_PD, 0);
#endif
        initHdmi();
#endif
        los_SetupSourceCommit();
    }
}

void los_stop(void)
{
    PROFILE_ADD(PROFILE_ID_LOS_STOP, 0, 0);

    assert(gctx.ref_cnt);

    gctx.ref_cnt--;
    if (!gctx.ref_cnt) {
        icTeardown(gctx.ctrl);
        pthread_join(eventThread, NULL);

#ifndef STREAM_ON_MIPI
        sendOutFini();
#else
        invalidate_tx_clients();

        mipi_tx_thread_destroy();
        pool_destroy(gctx.mipi_header_pool);
#endif

        icClientDestroy();
        osal_mutex_destroy(gctx.lock);
        sem_destroy(&semWaitForSourceStoped);
        sem_destroy(&semWaitForSourceReady);
        sem_destroy(&semWaitForSourceCommit);
        sem_destroy(&semWaitForLrtReady);
    }
}

void los_ConfigureGlobal(void *gconf)
{
    PROFILE_ADD(PROFILE_ID_LOS_GOB_CONFIG, gconf, 0);
}

void los_ConfigureSource(int srcIdx, icSourceConfig *sconf, int pipeId)
{
//    osal_mutex_lock(gctx.lock);
#ifdef STREAM_ON_MIPI
    //Configure tx clients for source
    init_tx_clients_for_source(srcIdx, sconf, pipeId);
#endif
    //Init source in RT
    PROFILE_ADD(PROFILE_ID_LOS_SRC_CONFIG, sconf, srcIdx);
    icConfigureSource(gctx.ctrl, (icSourceInstance)srcIdx, sconf);
//    osal_mutex_unlock(gctx.lock);
}
void los_SetupSource(int srcIdx, icSourceSetup *sconf)
{
//    osal_mutex_lock(gctx.lock);
    PROFILE_ADD(PROFILE_ID_LOS_SRC_CONFIG, sconf, srcIdx);
    icSetupSource (gctx.ctrl, (icSourceInstance)srcIdx, sconf);
//    osal_mutex_unlock(gctx.lock);
}
void los_SetupSourceCommit(void)
{
    osal_mutex_lock(gctx.lock);
    /* TODO: PROFILE_ADD */
    icSetupSourcesCommit(gctx.ctrl);
    assert(sem_wait(&semWaitForSourceCommit) != -1);
    osal_mutex_unlock(gctx.lock);
}

void los_ipipe_LockZSL(uint32_t srcIdx) {
    PROFILE_ADD(PROFILE_ID_LOS_LOCK_ZSL, 0, srcIdx);
    icLockZSL (gctx.ctrl, srcIdx, 0, IC_LOCKZSL_TS_RELATIVE);
}

// Dummy type
typedef struct
{
    uint32_t    dummy1;
    uint32_t    infoDatasSize;
    void        *dummy2;
} dummy_dbg_iq_framet_t;

void los_send_iq_debug(void *iq_data, uint32_t srcIdx)
{
    // Save and parse this ISP dump buffer:
    printf("isp debug data (cam: %lu) address: %p size: %lu\n", srcIdx, iq_data, ((dummy_dbg_iq_framet_t *)iq_data)->infoDatasSize);
#ifdef SEND_ISP_DATA_OVER_MIPI
    mipi_tx_thread_send(
            gctx.per_instance[srcIdx].client[FRAME_DATA_TYPE_IQ_DEBUG_DATA][FRAME_T_FORMAT_BINARY],
            update_frame_header(
                    pool_alloc(gctx.mipi_header_pool),
                    srcIdx,
                    FRAME_DATA_TYPE_IQ_DEBUG_DATA,
                    FRAME_T_FORMAT_BINARY
                ),
                iq_data,
                NULL,
                NULL
        );
#endif
}

void los_ipipe_TriggerCapture(void* buff, void *iconf, uint32_t srcIdx) {
    icIspConfig *ispCfg = (icIspConfig*)iconf;
    PROFILE_ADD(PROFILE_ID_LOS_TRIGGER_CAPTURE, buff, srcIdx);

    ispCfg->updnCfg0.vN   = 1;
    ispCfg->updnCfg0.vD   = 1;
    ispCfg->updnCfg0.hN   = 1;
    ispCfg->updnCfg0.hD   = 1;

    icTriggerCapture (gctx.ctrl, srcIdx, buff, iconf, 0);
}

void los_configIsp(void *iconf, int ispIdx)
{
    icIspConfig *ispCfg = (icIspConfig*)iconf;
//    osal_mutex_lock(gctx.lock);
    PROFILE_ADD(PROFILE_ID_LOS_CONFIG_ISP, ((icIspConfig *)iconf)->userData, ispIdx);

    if (QUARTER_MODE_FOR_VIDEO_PIPE == srcSet.appSpecificInfo) {
        ispCfg->updnCfg0.vN   = 1;
        ispCfg->updnCfg0.vD   = 2;
        ispCfg->updnCfg0.hN   = 1;
        ispCfg->updnCfg0.hD   = 2;
    } else {
        ispCfg->updnCfg0.vN   = 1;
        ispCfg->updnCfg0.vD   = 1;
        ispCfg->updnCfg0.hN   = 1;
        ispCfg->updnCfg0.hD   = 1;
    }

    icConfigureIsp(gctx.ctrl, ispIdx, iconf);
//    osal_mutex_unlock(gctx.lock);
}

int los_startSource(int srcIdx)
{
#ifdef STREAM_ON_MIPI
    pthread_mutex_init(&gctx.per_instance[srcIdx].lock, NULL);
    pthread_cond_init(&gctx.per_instance[srcIdx].cond, NULL);
    gctx.per_instance[srcIdx].wait_to_send_in_mipi_tx = 0;
#endif
    osal_mutex_lock(gctx.lock);
    PROFILE_ADD(PROFILE_ID_LOS_SRC_SART, 0, srcIdx);
    icStartSource(gctx.ctrl, (icSourceInstance)srcIdx);
    assert(sem_wait(&semWaitForSourceReady) != -1);
    osal_mutex_unlock(gctx.lock);

    return 0;
}

int los_stopSource(int srcIdx)
{
    osal_mutex_lock(gctx.lock);
    PROFILE_ADD(PROFILE_ID_LOS_SRC_STOP, 0, srcIdx);
    icStopSource(gctx.ctrl, (icSourceInstance)srcIdx);
    assert(sem_wait(&semWaitForSourceStoped) != -1);
    osal_mutex_unlock(gctx.lock);

#ifdef STREAM_ON_MIPI
    pthread_mutex_lock(&gctx.per_instance[srcIdx].lock);
    while (gctx.per_instance[srcIdx].wait_to_send_in_mipi_tx) {
        pthread_cond_wait(
                &gctx.per_instance[srcIdx].cond,
                &gctx.per_instance[srcIdx].lock
            );
    }
    pthread_mutex_unlock(&gctx.per_instance[srcIdx].lock);

    pthread_cond_destroy(&gctx.per_instance[srcIdx].cond);
    pthread_mutex_destroy(&gctx.per_instance[srcIdx].lock);
    //deregister_tx_clients_for_camera(srcIdx);
#endif

    return 0;
}

void los_dataWasSent(void *dataBufStruct)
{
    PROFILE_ADD(
            PROFILE_ID_LRT_DATA_SENT,
            ((FrameT *)dataBufStruct)->appSpecificData,
            0
        );

    icDataReceived(gctx.ctrl, dataBufStruct);
}

void openMetaMipiClient(void* hndl, uint32_t dataType, uint32_t dataSizeMax, int instance)
{
    UNUSED(dataType);
#ifdef DO_NOT_SEND_METADATA
    icMipiTxClientRegistered(instance, NULL, dataSizeMax, hndl);
#else
    void *client = init_meta_client_for_source(instance, dataSizeMax);
    icMipiTxClientRegistered(instance, client, dataSizeMax, hndl);
#endif
    return;
}

void closeMetaMipiClient(void* hndl, int instance)
{
#ifndef DO_NOT_SEND_METADATA
    mipi_tx_thread_client_unregister(&gctx.per_instance[instance].client[FRAME_DATA_TYPE_METADATA_PRV][FRAME_T_FORMAT_BINARY]);
#endif
    icMipiTxClientUnregistered(instance, NULL, hndl);
    return;
}

void sendMetaTxClientData(void* hndl, void * inBuffer, uint32_t buffSize, int instance)
{
#ifdef DO_NOT_SEND_METADATA
    icMipiTxClientDataSent(instance, NULL, buffSize, inBuffer, hndl);
#else
    pthread_mutex_lock(&gctx.per_instance[instance].lock);
    gctx.per_instance[instance].wait_to_send_in_mipi_tx++;
    pthread_mutex_unlock(&gctx.per_instance[instance].lock);

    assert(!mipi_tx_thread_send(
            gctx.per_instance[instance].client[FRAME_DATA_TYPE_METADATA_PRV][FRAME_T_FORMAT_BINARY],
            update_frame_header(
                    pool_alloc(gctx.mipi_header_pool),
                    instance,
                    FRAME_DATA_TYPE_METADATA_PRV,
                    FRAME_T_FORMAT_BINARY
                ),
            inBuffer,
            NULL,
            hndl
        ));
#endif
    return ;
}
