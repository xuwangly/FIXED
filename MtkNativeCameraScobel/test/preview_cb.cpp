#include <utils/Log.h>
#include <cutils/memory.h>
#include <unistd.h>
//
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
//
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/IGraphicBufferProducer.h>
#include <gui/ISurfaceComposer.h>
#include <ui/DisplayInfo.h>
#include <android/native_window.h>
//
#include "inc/CamLog.h"
#include "inc/Utils.h"
#include "inc/Command.h"
#include <camera/ICamera.h>
#include <camera/CameraParameters.h>
#include <camera/MtkCameraParameters.h>
//
#include "inc/CamLog.h"
#include "inc/Utils.h"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgproc.hpp";
#include "opencv2/imgcodecs.hpp";
#include "opencv2/core.hpp"
#include <stdio.h>
#include <vector>
  
using namespace cv;
using namespace android;
using namespace std;

class Preview_cb : public CameraListener
{
public:
    Preview_cb(){}
    ~Preview_cb(){}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  CameraListener Interface.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
public:     ////                Interface.
    virtual void                notify(int32_t msgType, int32_t ext1, int32_t ext2) {}
    virtual void                postData(int32_t msgType, const sp<IMemory>& dataPtr, camera_frame_metadata_t *metadata);
    virtual void                postDataTimestamp(nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr) {}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Implementations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
public:     ////
    struct Argument : LightRefBase<Argument>
    {
        String8                     ms8AppMode;
        int32_t                     mi4CamMode;
        int32_t                     mOpenId;
        android::Size               mPreviewSize;
        bool                        mDisplayOn;
        int32_t                     mDisplayOrientation;
        String8                     ms8PrvFmt;
    };
private:
    sp<Argument>                mpArgument;
    sp<Camera>      mpCamera;
protected:  ////                Operations (Surface)
    virtual bool                initSurface();
    virtual void                uninitSurface();

protected:  ////                Data Members (Surface)
    int32_t                     mi4SurfaceID;
    sp<SurfaceComposerClient>   mpSurfaceClient;
    sp<SurfaceControl>          mpSurfaceControl;
    sp<Surface>                 mpSurface;
    sp<IGraphicBufferProducer>  mpGbp;
    ANativeWindow*              mpWindow;
public:
    bool init();
    bool initCamera();
    bool unInitCamera();
    int  doSobel(uint8_t *const buf, uint32_t const size);
};

bool
Preview_cb::
init()
{
    //打开校准图片
    initSurface();
    //open camera
    initCamera();
    usleep(1000*1000*30);
    return true;
}
bool
Preview_cb::
initSurface()
{
    mi4SurfaceID = 0;

    // create a client to surfaceflinger
    mpSurfaceClient = new SurfaceComposerClient();

    sp<IBinder> display(SurfaceComposerClient::getBuiltInDisplay(
                ISurfaceComposer::eDisplayIdMain));
    DisplayInfo info;
    SurfaceComposerClient::getDisplayInfo(display, &info);
    ssize_t displayWidth = info.w;
    ssize_t displayHeight = info.h;
    CAM_LOGD("displayWidth:%d displayHeight:%d\n" ,displayWidth, displayHeight);
    mpSurfaceControl = mpSurfaceClient->createSurface(
        String8("surface"), displayWidth, displayHeight, PIXEL_FORMAT_RGB_565, 0
    );
    mpSurface = mpSurfaceControl->getSurface();
    mpWindow = mpSurface.get();
    SurfaceComposerClient::openGlobalTransaction();
    mpSurfaceControl->setLayer(100000);
    SurfaceComposerClient::closeGlobalTransaction();

    ANativeWindow_Buffer outBuffer;
    mpSurface->lock(&outBuffer, NULL);
    displayWidth = outBuffer.stride;
    ssize_t bpr = outBuffer.stride * bytesPerPixel(outBuffer.format);
    uint16_t *ptr = (uint16_t *)outBuffer.bits;
    for(int i = 0; i < displayWidth ; i++){
        for(int j = 0; j <  displayHeight ; j++){
            if(     i > 100 && i <  110) 
                *ptr = 0x0;
            else if(i > 200 && i <  210 )
                *ptr = 0x0;
            else if(i > 300 && i <  310 )
                *ptr = 0x0;
            else if(i > 400 && i <  410 )
                *ptr = 0x0;
            else if(i > 500 && i <  510 )
                *ptr = 0x0;
            else if(i > 600 && i <  610 )
                *ptr = 0x0;
            else if(i > 700 && i <  710 )
                *ptr = 0x0;
            else
                *ptr = 0xffff;
            ptr++;
        }
    }
    CAM_LOGD("stride: %d bytesPerPixel:%d outBuffer.height%d  %d  %d", outBuffer.stride , bytesPerPixel(outBuffer.format) ,outBuffer.height , displayWidth ,displayHeight );
    //android_memset16((uint16_t *)outBuffer.bits, 0xffff, bpr*outBuffer.height);
    mpSurface->unlockAndPost();
    
    CAM_LOGD("setupSurface: %p", mpSurface.get());
    return  (mpSurface != 0);
}
void
Preview_cb::
uninitSurface()
{
    mpWindow = NULL;
    mpSurface = 0;
    mpSurfaceControl = 0;
    mpSurfaceClient = 0;
}
bool
Preview_cb::
initCamera()
{
    mpArgument = new Argument;
    mpArgument->ms8AppMode = String8(MtkCameraParameters::APP_MODE_NAME_DEFAULT);
    mpArgument->mi4CamMode = MtkCameraParameters::CAMERA_MODE_MTK_PRV;
    mpArgument->mOpenId = 0;
    mpArgument->mPreviewSize = android::Size(640, 480);
    mpArgument->mDisplayOrientation = 90;
    mpArgument->mDisplayOn = true;
    mpArgument->ms8PrvFmt = String8(CameraParameters::PIXEL_FORMAT_YUV420P);

    //connect camera
    status_t status = OK;
    status = Camera::setProperty(String8(MtkCameraParameters::PROPERTY_KEY_CLIENT_APPMODE), mpArgument->ms8AppMode);
    MY_LOGD("status(%d), app-mode=%s, format=%s", status, mpArgument->ms8AppMode.string(), mpArgument->ms8PrvFmt.string());

    mpCamera = Camera::connect(mpArgument->mOpenId, String16("CamTest"), Camera::USE_CALLING_UID);
    if  ( mpCamera == 0 )
    {
        MY_LOGE("Camera::connect, id(%d)", mpArgument->mOpenId);
        return  false;
    }
    mpCamera->setListener(this);
    MY_LOGD("Camera::connect, id(%d), camera(%p)", mpArgument->mOpenId, mpCamera.get());
    //end connect

    //setupParameters
    CameraParameters params(mpCamera->getParameters());
    params.set(MtkCameraParameters::KEY_CAMERA_MODE, mpArgument->mi4CamMode);
    params.setPreviewSize(mpArgument->mPreviewSize.width, mpArgument->mPreviewSize.height);
    params.set(CameraParameters::KEY_PREVIEW_FORMAT, mpArgument->ms8PrvFmt.string());
    if  (OK != mpCamera->setParameters(params.flatten()))
    {
        CAM_LOGE("setParameters\n");
        return  false;
    }
    //end set
    #if 1
        mpCamera->setPreviewCallbackFlags(CAMERA_FRAME_CALLBACK_FLAG_ENABLE_MASK);
    #endif

    mpCamera->startPreview();
    //mpCamera->stopPreview();
    return true;
}
bool
Preview_cb::
unInitCamera()
{
    if  ( mpCamera != 0 ){
        mpCamera->stopPreview();
        CAM_LOGE("Camera::disconnect, camera(%p)", mpCamera.get());
        mpCamera->disconnect();
        mpCamera = NULL;
    }
    return true;
}
void
Preview_cb::
postData(int32_t msgType, const sp<IMemory>& dataPtr, camera_frame_metadata_t *metadata)
{
    ssize_t offset;
    size_t size;
    sp<IMemoryHeap> heap = dataPtr->getMemory(&offset, &size);
    uint8_t* pBase = (uint8_t *)heap->base() + offset;

    //MY_LOGD("msgType=%x CAMERA_MSG_PREVIEW_FRAME?%d base/size=%p/%d", msgType, (msgType & CAMERA_MSG_PREVIEW_FRAME), pBase, size);
    if  ( 0 == (msgType & CAMERA_MSG_PREVIEW_FRAME) )
    {
        MY_LOGD("CAMERA_MSG_PREVIEW_FRAME\n");
        return;
    }
    doSobel(pBase, size);

    #if 0
    static int i = 0;
    i++;
    String8 filename = String8::format("sdcard/prv%dx%d_%02d.yuv", mpArgument->mPreviewSize.width, mpArgument->mPreviewSize.height, i);
    saveBufToFile(filename, pBase, size);
    #endif
}

int 
Preview_cb::
doSobel(uint8_t *const buf, uint32_t const size)
{
    static int i = 0;
    i++;
    cv::Mat yuvImg;
    cv::Mat rgbImg;
    cv::Mat imageGrey;
    double meanValueS = 0.0;
    double meanValueL = 0.0;
    double meanValueStd = 0.0;

    //
    yuvImg.create(mpArgument->mPreviewSize.height*3/2, mpArgument->mPreviewSize.width, CV_8UC1);
    memcpy(yuvImg.data, buf, size*sizeof(unsigned char));
    cvtColor(yuvImg, rgbImg, CV_YUV2BGR_I420);
    cvtColor(rgbImg, imageGrey, CV_RGB2GRAY);
    Mat imageGreySmall;
    //截取preview中的一部分图片进行分析
    imageGrey(Range(50,250),Range(220,420)).copyTo(imageGreySmall);
    Mat imageSobel , imageLaplacian;

    //Sobel算法:只计算y轴的Sobel
    Sobel(imageGreySmall, imageSobel, imageGreySmall.depth(), 0, 1);
    meanValueS = mean(imageSobel)[0];

    //Laplacian算法
    //Laplacian(imageGrey, imageLaplacian, imageGrey.depth());
    //图像的平均灰度   
    //meanValueL = mean(imageLaplacian)[0];

    //StdDev算法
    //Mat meanValueImage;  
    //Mat meanStdValueImage;
    //meanStdDev(imageGrey, meanValueImage, meanStdValueImage);  
    //meanValueStd = meanStdValueImage.at<double>(0, 0);


    if(i == 1){
        printf("yuvImg:        %4d %4d %4d %4d %4d\n" ,yuvImg.rows , yuvImg.cols , yuvImg.channels() , yuvImg.isContinuous() , yuvImg.depth());
        printf("rgbImg:        %4d %4d %4d %4d %4d\n" ,rgbImg.rows , rgbImg.cols , rgbImg.channels() , rgbImg.isContinuous() , rgbImg.depth());
        printf("imageGrey:     %4d %4d %4d %4d %4d\n" ,imageGrey.rows , imageGrey.cols , imageGrey.channels() , imageGrey.isContinuous() , imageGrey.depth());
        printf("imageGreySmall:%4d %4d %4d %4d %4d\n" ,imageGreySmall.rows , imageGreySmall.cols , imageGreySmall.channels() , imageGreySmall.isContinuous() , imageGreySmall.depth());
        printf("imageSobel:    %4d %4d %4d %4d %4d\n" ,imageSobel.rows , imageSobel.cols , imageSobel.channels() , imageSobel.isContinuous() , imageSobel.depth());
        saveBufToFile("sdcard/LOLyuvImg.yuv", yuvImg.data, size);
        saveBufToFile("sdcard/LOLyuvImgrgbImg.rgb", rgbImg.data, size/3*2*3);
        saveBufToFile("sdcard/LOLyuvImgimageGrey.yuv", imageGrey.data, size/3*2);
        saveBufToFile("sdcard/LOLyuvimageGreySmall.yuv", imageGreySmall.data, imageGreySmall.rows*imageGreySmall.cols);
        saveBufToFile("sdcard/LOLyuvImgimageSobel.yuv", imageSobel.data, imageSobel.rows*imageSobel.cols);
    }
    printf("%4f  %4f  %4f\n" , meanValueS , meanValueL , meanValueStd);
    return 0;
}

int main(int argc, char const *argv[])
{
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    sp <Preview_cb> preview_cb = new Preview_cb();
    preview_cb->init();

    IPCThreadState::self()->joinThreadPool();
    return 0;
}