#ifdef SUBCORE
#error "Core selection is wrong!!"
#endif

#include <LowPower.h>

#include <MP.h>
#include <MPMutex.h>
MPMutex mutex(MP_MUTEX_ID0);

#include <Camera.h>

const int subcore = 1;   // サブコア1を使用
const int imgBufferSize = (320 * 240 * 2) + 4;

static unsigned char imgBuffer[imgBufferSize + 1];

void CamCB(CamImage img) {
  int8_t sndid = 100;

  // MPLog("CamCB: Start\n");
  if (img.isAvailable()) {
    MPLog("CamCB: Available\n");
    CamImage imgTmp(img), imgSend;
    CamErr e;

    img.convertPixFormat(CAM_IMAGE_PIX_FMT_RGB565);

    e = imgTmp.convertPixFormat(CAM_IMAGE_PIX_FMT_YUV422);
    if(e != CAM_ERR_SUCCESS){
        MPLog("CamErr: convertPixFormat CAM_IMAGE_PIX_FMT_YUV422 %d\n", e);
    }

    e = imgTmp.resizeImageByHW(imgSend, imgTmp.getWidth()/2, imgTmp.getHeight()/2);
    if(e != CAM_ERR_SUCCESS){
        MPLog("CamErr: resizeImageByHW %d\n", e);
    }

    e = imgSend.convertPixFormat(CAM_IMAGE_PIX_FMT_GRAY);
    if(e != CAM_ERR_SUCCESS){
        MPLog("CamErr: convertPixFormat CAM_IMAGE_PIX_FMT_GRAY %d\n", e);
    }

    int32_t imgsize = imgSend.getImgSize();
    int16_t height = imgSend.getHeight();
    int16_t width = imgSend.getWidth();
    if(imgsize > 0){
      MPLog("CamCB: mutex.Trylock\n");
      if(mutex.Trylock() == 0){
        MPLog("CamCB: locked\n");
        memcpy(imgBuffer+8, imgSend.getImgBuff(), imgsize);
        imgBuffer[0] = ((imgsize >> 24) & 0xFF);
        imgBuffer[1] = ((imgsize >> 16) & 0xFF);
        imgBuffer[2] = ((imgsize >> 8) & 0xFF);
        imgBuffer[3] = (imgsize & 0xFF);
        imgBuffer[4] = ((width >> 8) & 0xFF);
        imgBuffer[5] = (width & 0xFF);
        imgBuffer[6] = ((height >> 8) & 0xFF);
        imgBuffer[7] = (height & 0xFF);

        mutex.Unlock();

        MPLog("CamCB, Size: %d, width: %d, height: %d\n", imgsize, width, height);

        int ret = MP.Send(sndid, imgBuffer, subcore);
        if(ret < 0){
          MPLog("MP.Send error: %d\n", ret);
        }
      }
    }
  } else {
    MPLog("Image is not available!\n");
  }
}

void setup() {
  theCamera.begin();
  // setStillPictureImageFormat() .
  // theCamera.startStreaming(true, CamCB); // カメラのストリーミングを開始
    theCamera.setStillPictureImageFormat(
     CAM_IMGSIZE_QVGA_H,
     CAM_IMGSIZE_QVGA_V,
     CAM_IMAGE_PIX_FMT_YUV422);
  Serial.begin(9600);
  
  LowPower.begin();
  LowPower.clockMode(CLOCK_MODE_32MHz);
  bootcause_e bc = LowPower.bootCause(); /* get boot cause */

  if ((bc == POR_SUPPLY) || (bc == POR_NORMAL)) {
    MPLog("Power on reset: %d\n", bc);
  } else {
    MPLog("Wakeup from deep sleep: %d\n", bc);
  }

  int ret = MP.begin(subcore);  // サブコア1を起動
  if(ret < 0){
    MPLog("MP.begin error: %d\n", ret);
  }
  MP.RecvTimeout(MP_RECV_BLOCKING); 
}

void loop() {
  MPLog("loop start...\n");
  sleep(1); 
  CamImage img = theCamera.takePicture();
  MPLog("took picture..\n");
  CamCB(img);
  
  int8_t msgid, subid;  // main core message id
  int8_t send_status = 0;
  int ret;
  ret = MP.Recv(&msgid, &send_status, 1);
  LowPower.deepSleep(60);
  MPLog("loop end...\n");
}
