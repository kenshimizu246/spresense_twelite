#ifdef SUBCORE
#error "Core selection is wrong!!"
#endif

#include <LowPower.h>

#include <MP.h>
#include <MPMutex.h>
MPMutex mutex(MP_MUTEX_ID0);

#include <Camera.h>
#include "Adafruit_ILI9341.h"

#define TFT_DC 9
#define TFT_CS 10
Adafruit_ILI9341 display = Adafruit_ILI9341(TFT_CS, TFT_DC);

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
    display.drawRGBBitmap(0, 0 /* 開始座標 */
        , (uint16_t*)img.getImgBuff() /* 画像データ */ 
        , 320, 240); /* 横幅、縦幅  */

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
  }
}

void setup() {
  display.begin(); //　液晶ディスプレイの開始
  theCamera.begin(); // カメラの開始
  display.setRotation(3); // ディスプレイの向きを設定
  theCamera.startStreaming(true, CamCB); // カメラのストリーミングを開始
  Serial.begin(9600);

  int ret = MP.begin(subcore);  // サブコア1を起動
  if(ret < 0){
    MPLog("MP.begin error: %d\n", ret);
  }
}

void loop() {
}
