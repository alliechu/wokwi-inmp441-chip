/*
 * ESP32 IDF Example: Stream INMP441 audio over serial as binary
 *
 * Protocol:
 *   - Sends a 4-byte sync header: 0xAA 0xBB 0xCC 0xDD
 *   - Followed by a 4-byte little-endian sample count (uint32)
 *   - Followed by raw int16 samples (2 bytes each, little-endian)
 *   - One packet per buffer read (64 samples = 128 bytes of audio)
 *
 * SPDX-License-Identifier: MIT
 * Copyright 2023 Allie Chung
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "driver/i2s.h"
#include "driver/uart.h"

#define I2S_BCK 26
#define I2S_WS 25
#define I2S_DATA 22

#define SAMPLE_RATE 8000
#define BUF_SAMPLES 64
#define SERIAL_BAUD 230400

// Sync header sent before every packet so Python can re-sync if bytes are lost.
// static const uint8_t SYNC_HEADER[4] = {0xAA, 0xBB, 0xCC, 0xDD};

void app_main(void) {
  // High baud rate for raw binary streaming.
  // idf.py monitor won't be readable after this - use the Python script instead.
  uart_set_baudrate(UART_NUM_0, SERIAL_BAUD);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUF_SAMPLES,
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

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_start(I2S_NUM_0);

  esp_log_level_set("*", ESP_LOG_NONE); // silence ESP-IDF logs
  uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(200)); // flush pending output
  uart_set_baudrate(UART_NUM_0, SERIAL_BAUD);
  vTaskDelay(pdMS_TO_TICKS(50)); // let rate change settle
  uart_set_baudrate(UART_NUM_0, 921600);

  //vTaskDelay(pdMS_TO_TICKS(200)); // let mic stabilize

  int32_t raw_buf[BUF_SAMPLES];
  int16_t out_buf[BUF_SAMPLES];
  uint32_t total_samples = 0;

  while (1) {
    size_t bytes_read = 0;
    i2s_read(I2S_NUM_0, raw_buf, sizeof(raw_buf), &bytes_read, pdMS_TO_TICKS(1000));

    uint32_t n = bytes_read / 4;
    if(n == 0) continue;

    // Convert 32-bit left-justified samples to 16-bit.
    for(uint32_t i = 0; i < n; i++) {
      out_buf[i] = (int16_t)(raw_buf[i] >> 16);
    }

    // Write packet: [sync header][sample count uint32][int16 samples]
    // fwrite(SYNC_HEADER, 1, 4, stdout);
    // fwrite(&n, 4, 1, stdout);
    // fwrite(out_buf, 2, n, stdout);
    // fflush(stdout);
    if (n > 0) {
      uart_write_bytes(UART_NUM_0, (const char*)out_buf, n * 2);
    }

    total_samples += n;
  }
}