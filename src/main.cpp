// Copyright(c) 2023 Takao Akaki


#include <M5Unified.h>
#include <Avatar.h>
#include "fft.hpp"

#define USE_MIC

#ifdef USE_MIC
  // ---------- Mic sampling ----------

  int pin_clk  = -1;
  int pin_data = -1;
  //#define PIN_CLK     0
  //#define PIN_DATA    34
  #define READ_LEN    (2 * 256)
  #define GAIN_FACTOR 3
  uint8_t BUFFER[READ_LEN] = {0};

  int16_t *adcBuffer = NULL;
  static fft_t fft;
  static constexpr size_t WAVE_SIZE = 320;
  static int16_t raw_data[WAVE_SIZE * 2];

  float lipsync_max = 300.0f;  // リップシンクの単位ここを増減すると口の開き方が変わります。

#endif




using namespace m5avatar;

Avatar avatar;
ColorPalette *cps[6];
uint8_t palette_index = 0;

uint32_t last_rotation_msec = 0;

void initI2S()  // Init I2S.  初始化I2S
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX |
                             I2S_MODE_PDM),  // Set the I2S operating mode.
                                             // 设置I2S工作模式
        .sample_rate = 44100,  // Set the I2S sampling rate.  设置I2S采样率
        .bits_per_sample =
            I2S_BITS_PER_SAMPLE_16BIT,  // Fixed 12-bit stereo MSB.
                                        // 固定为12位立体声MSB
        .channel_format =
            I2S_CHANNEL_FMT_ALL_RIGHT,  // Set the channel format.  设置频道格式
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 1, 0)
        .communication_format =
            I2S_COMM_FORMAT_STAND_I2S,  // Set the format of the communication.
#else                                   // 设置通讯格式
        .communication_format = I2S_COMM_FORMAT_I2S,
#endif
        .intr_alloc_flags =
            ESP_INTR_FLAG_LEVEL1,  // Set the interrupt flag.  设置中断的标志
        .dma_buf_count = 2,        // DMA buffer count.  DMA缓冲区计数
        .dma_buf_len   = 128,      // DMA buffer length.  DMA缓冲区长度
    };

    i2s_pin_config_t pin_config;

#if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 3, 0))
    pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
#endif

    pin_config.bck_io_num   = I2S_PIN_NO_CHANGE;
    pin_config.ws_io_num    = pin_clk;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config.data_in_num  = pin_data;

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_set_clk(I2S_NUM_0, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
}

int8_t i2s_readraw_buff[1024];
uint8_t fft_dis_buff[161][80] = {0};
uint16_t posData              = 160;

void lipsync() {
  
  size_t bytesread;
  uint64_t level = 0;
  i2s_read(I2S_NUM_0, (char *)i2s_readraw_buff, 1024, &bytesread,
              (100 / portTICK_RATE_MS));
  adcBuffer = (int16_t *)i2s_readraw_buff;
  fft.exec(adcBuffer);
  for (size_t bx=5;bx<=60;++bx) {
    int32_t f = fft.get(bx);
    level += abs(f);
  }

  //Serial.printf("level:%d\n", level) ;         // lipsync_maxを調整するときはこの行をコメントアウトしてください。
  float ratio = (float)((level >> 9)/lipsync_max);
  // Serial.printf("ratio:%f\n", ratio);
  if (ratio <= 0.01f) {
    ratio = 0.0f;
  } else if (ratio >= 1.3f) {
    ratio = 1.3f;
  }
  if ((millis() - last_rotation_msec) > 350) {
    int direction = random(2);
    if (direction == 1) {
      avatar.setRotation(20 * ratio);
    } else {
      avatar.setRotation(-20 * ratio);
    }
    last_rotation_msec = millis();
  }
  avatar.setMouthOpenRatio(ratio);
}


void setup()
{
  auto cfg = M5.config();
  cfg.internal_mic = false;
  M5.begin(cfg);
  float scale = 0.0f;
  int8_t position_x = 0;
  int8_t position_y = 0;
  uint8_t display_rotation = 3; // ディスプレイの向き(0〜3)
  uint8_t first_cps = 0;
  switch (M5.getBoard()) {
    case m5::board_t::board_M5AtomS3:
      first_cps = 4;
      scale = 0.5f;
      position_x = 5;
      position_y = -15;
      pin_clk  = 1;
      pin_data = 2;
      break;

    case m5::board_t::board_M5StickC:
      M5.Power.Axp192.setLDO0(2800); // 一部これを実行しないとマイクが動かない機種がある。
      first_cps = 1;
      scale = 0.6f;
      position_x = -30;
      position_y = -15;
      pin_clk  = 0;
      pin_data = 34;
      break;

    case m5::board_t::board_M5StickCPlus:
      M5.Power.Axp192.setLDO0(2800); // 一部これを実行しないとマイクが動かない機種がある。
      first_cps = 2;
      scale = 0.7f;
      position_x = -15;
      position_y = 5;
      pin_clk  = 0;
      pin_data = 34;
      break;
    
    case m5::board_t::board_M5Stack:
      scale = 1.0f;
      position_x = 0;
      position_y = 0;
      display_rotation = 1;
      break;

    defalut:
      Serial.println("Invalid board.");
      break;
  }
  initI2S();
  M5.Lcd.setRotation(display_rotation);
  avatar.setScale(scale);
  avatar.setPosition(position_x, position_y);
  avatar.init(); // start drawing
  cps[0] = new ColorPalette();
  cps[0]->set(COLOR_PRIMARY, TFT_BLACK);
  cps[0]->set(COLOR_BACKGROUND, TFT_YELLOW);
  cps[1] = new ColorPalette();
  cps[1]->set(COLOR_PRIMARY, TFT_BLACK);
  cps[1]->set(COLOR_BACKGROUND, TFT_ORANGE);
  cps[2] = new ColorPalette();
  cps[2]->set(COLOR_PRIMARY, (uint16_t)0x00ff00);
  cps[2]->set(COLOR_BACKGROUND, (uint16_t)0x303303);
  cps[3] = new ColorPalette();
  cps[3]->set(COLOR_PRIMARY, TFT_WHITE);
  cps[3]->set(COLOR_BACKGROUND, TFT_BLACK);
  cps[4] = new ColorPalette();
  cps[4]->set(COLOR_PRIMARY, TFT_BLACK);
  cps[4]->set(COLOR_BACKGROUND, TFT_WHITE);
  cps[5] = new ColorPalette();
  cps[5]->set(COLOR_PRIMARY, 0x303303);
  cps[5]->set(COLOR_BACKGROUND, 0x00ff00);
  avatar.setColorPalette(*cps[first_cps]);
  //avatar.addTask(lipsync, "lipsync");
  last_rotation_msec = millis();
}

uint32_t count = 0;

void loop()
{
  M5.update();
  if (M5.BtnA.wasPressed()) {
    palette_index++;
    if (palette_index > 5) {
      palette_index = 0;
    }
    avatar.setColorPalette(*cps[palette_index]);
  }
  
//  if ((millis() - last_rotation_msec) > 100) {
    //float angle = 10 * sin(count);
    //avatar.setRotation(angle);
    //last_rotation_msec = millis();
    //count++;
  //}

  // avatar's face updates in another thread
  // so no need to loop-by-loop rendering
  lipsync();
  vTaskDelay(1/portTICK_PERIOD_MS);
}
