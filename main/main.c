#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "driver/i2s.h"

// define I2S pins for ESP32
#define I2S_BCK 26
#define I2S_WS 25
#define I2S_DATA 22

void app_main(void) {
  printf("Start\n");
  fflush(stdout);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_DATA
  };

  // esp_err_t err;

  // err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  // printf("install err: %d\n", err);

  // err = i2s_set_pin(I2S_NUM_0, &pin_config);
  // printf("pin err: %d\n", err);

  // err = i2s_start(I2S_NUM_0);
  // printf("start err: %d\n", err);
  // fflush(stdout);

  vTaskDelay(pdMS_TO_TICKS(200));  // let mic start

  int32_t buffer[64];
  uint32_t total_samples = 0;
  
  while (1) {
    size_t bytes_read = 0;

    err = i2s_read(
      I2S_NUM_0,
      buffer,
      sizeof(buffer),
      &bytes_read,
      pdMS_TO_TICKS(1000)
    );

    uint32_t samples_this_read = bytes_read / 4;
    total_samples += samples_this_read;

    printf("read err: %d, bytes_read: %u, samples: %lu, total: %lu\n", err, (unsigned)bytes_read,
           (unsigned long)samples_this_read, (unsigned long)total_samples);

    for (int i = 0; i < (int)samples_this_read; i++) {
    int32_t raw = buffer[i];
    int32_t sample = raw >> 8;  // extract 24-bit value
    printf("  [%lu] %ld\n",
           (unsigned long)(total_samples - samples_this_read + i),
           (long)sample);
    } 
  
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}