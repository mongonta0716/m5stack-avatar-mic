// Copyright(c) 2023 Takao Akaki


#include <M5Unified.h>
#include <Avatar.h>
#include "fft.hpp"

#define USE_MIC

#ifdef USE_MIC
  // ---------- Mic sampling ----------
static constexpr const size_t record_number = 24;
static constexpr const size_t record_length = 256;
static constexpr const size_t record_size = record_number * record_length;
static constexpr const size_t record_samplerate = 16000;
static int16_t prev_y[record_length];
static int16_t prev_h[record_length];
static size_t rec_record_idx = 2;
static size_t draw_record_idx = 0;
static int16_t* rec_data;

static fft_t fft;
float lipsync_max = 10.0f;  // リップシンクの単位ここを増減すると口の開き方が変わります。
float last_ratio = 0.0f;
uint64_t offset = 255; // 無音時のレベル

#endif




using namespace m5avatar;

Avatar avatar;
ColorPalette *cps[6];
uint8_t palette_index = 0;

uint32_t last_rotation_msec = 0;


void lipsync() {
  
  size_t bytesread;
  uint64_t level = 0;
  if (M5.Mic.record(rec_data, record_length, record_samplerate)) {

    fft.exec(rec_data);

  }
  for (size_t bx=5;bx<=60;++bx) {
    int32_t f = fft.get(bx);
    level += abs(f);
  }

  Serial.printf("level:%d: max:%f: offset:%d\n", level >> 16, lipsync_max, offset) ;         // lipsync_maxを調整するときはこの行をコメントアウトしてください。
  if ((level >> 16) != 0) {
    offset = min(offset, level >> 16);
  }
  
  float ratio = (float)(((level >> 16) - offset)/lipsync_max);
  Serial.printf("ratio:%f\n", ratio);
  if (ratio > 1.3f) {
    if (ratio > 1.6f) {
      lipsync_max += 10.0f;
      ratio = 1.3f;
    }
  } else if (ratio <= 0.6f) {
    lipsync_max -= 10.0f;
    if (lipsync_max < 10.0f) {
      lipsync_max = 10.0f;
    }
    if (ratio < 0.2f) {
      ratio = 0.0f;
    }
  } 

  if ((millis() - last_rotation_msec) > 350) {
    int direction = random(-2, 2);
    avatar.setRotation(direction * 10 * ratio);
    last_rotation_msec = millis();
  }
  avatar.setMouthOpenRatio(ratio);
  if (offset < 30) {
    offset = 255;
  }
}


void setup()
{
  auto cfg = M5.config();
  cfg.internal_mic = true;
  M5.begin(cfg);
  float scale = 0.0f;
  int8_t position_x = 0;
  int8_t position_y = 0;
  uint8_t display_rotation = 3; // ディスプレイの向き(0〜3)
  uint8_t first_cps = 0;
  auto mic_cfg = M5.Mic.config();
  switch (M5.getBoard()) {
    case m5::board_t::board_M5AtomS3:
      first_cps = 4;
      scale = 0.5f;
      position_x = 5;
      position_y = -15;
      display_rotation = 1; // ディスプレイの向き(0〜3)
      // PDMUnitの接続先を指定
      mic_cfg.pin_ws = 1;
      mic_cfg.pin_data_in = 2;
      mic_cfg.magnification = 15; // 口がうまく動かない場合はここを調節してください。
      //mic_cfg.noise_filter_level = 1;
      M5.Mic.config(mic_cfg);
      break;

    case m5::board_t::board_M5StickC:
      first_cps = 1;
      scale = 0.6f;
      position_x = -30;
      position_y = -15;
      break;

    case m5::board_t::board_M5StickCPlus:
      first_cps = 2;
      scale = 0.7f;
      position_x = -15;
      position_y = 5;
      break;
    
    case m5::board_t::board_M5StackCore2:
      scale = 1.0f;
      position_x = 0;
      position_y = 0;
      display_rotation = 1;
      break;

      
    defalut:
      Serial.println("Invalid board.");
      break;
  }
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
  rec_data = (typeof(rec_data))heap_caps_malloc(record_size * sizeof(int16_t), MALLOC_CAP_8BIT);
  memset(rec_data, 0, record_size * sizeof(int16_t));
  M5.Mic.begin();
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
  vTaskDelay(50/portTICK_PERIOD_MS);
}
