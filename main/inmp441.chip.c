// Wokwi Custom Chip - For docs and examples see:
// https://docs.wokwi.com/chips-api/getting-started
//
// SPDX-License-Identifier: MIT
// Copyright 2023 Allie Chung

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#define SAMPLE_RATE 16000

#define SIGNAL_SINE 0
#define SIGNAL_SQUARE 1
#define SIGNAL_NOISE 2

typedef struct {
  // chip variables
  pin_t sck_pin;
  pin_t ws_pin;
  pin_t sd_pin;
  uint32_t current_sample;
  int bit_count;
  int prev_ws;
  float phase;
  uint32_t signal_type;  // 0 = sine, 1 = square, 2 = noise
  uint32_t sample_count; // total samples generated
} inmp441_t;

static inmp441_t chip;

static int32_t next_sample() { // generates sine wave
  chip.sample_count++;
  chip.phase += 2.0f * 3.14159f * 440.0f / SAMPLE_RATE;
  if (chip.phase > 2.0f * 3.14159f) chip.phase -= 2.0f * 3.14159f;

  int32_t sample;
  switch (chip.signal_type) {
    case SIGNAL_SQUARE:
      sample = (chip.phase < 3.14159f) ? (int32_t)0x7FFFFF : (int32_t)-0x7FFFFF;
      break;
    case SIGNAL_NOISE:
      sample = (int32_t)(((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * (int32_t)0x7FFFFF;
      break;
    case SIGNAL_SINE:
    default:
      sample = (int32_t)(sinf(chip.phase) * (float)0x7FFFFF);
      break;
  }
  return (int32_t)((uint32_t)sample << 8); // left-justify in 32-bit word
}

static void clock_change(void *data, pin_t pin, uint32_t value) {
  // int clk = pin_read(chip.sck_pin);
  if(value != 0) { return; } // only capture on clock edge

  // mic outputs next bit of current audio sample
  int ws = pin_read(chip.ws_pin);
  if(ws != chip.prev_ws) { // new word starting
    chip.prev_ws = ws;
    chip.bit_count = -1;

    if(ws == 0) {
      chip.current_sample = (uint32_t)next_sample();
    }
    else {
      chip.current_sample = 0;
    }
    // chip.bit_count = 0;
    pin_write(chip.sd_pin, 0);
    return;
  }

  if(chip.bit_count < 0) {
    // 1-bit delay slot
    chip.bit_count = 0;
    pin_write(chip.sd_pin, 0);
    return;
  }

  if(chip.bit_count < 32) { // shift out MSB first, one bit per clock
  // 32 bits per word
    int bit = (chip.current_sample >> (31 - chip.bit_count)) & 1;
    pin_write(chip.sd_pin, bit);
    chip.bit_count++;
  }
  else {
    pin_write(chip.sd_pin, 0);
  }
}

void chip_init() {
  // initialize chip, setup IO pins
  chip.sck_pin = pin_init("BCK", INPUT);
  chip.ws_pin = pin_init("WS",  INPUT);
  chip.sd_pin = pin_init("DATA",  OUTPUT);
  pin_init("L/R", INPUT);
  pin_init("VCC", INPUT);
  pin_init("GND", INPUT);

  // read signal
  chip.signal_type  = attr_init("signal", SIGNAL_SINE);
  chip.sample_count = 0;
  
  chip.bit_count = 0;
  chip.current_sample = 0;
  chip.prev_ws = -1;
  chip.phase = 0.0f;

  const pin_watch_config_t watch = {
    .edge = BOTH,
    .pin_change = clock_change,
    .user_data = NULL,
  };
  pin_watch(chip.sck_pin, &watch);
}
