# I2S Timing Notes — INMP441 Custom Chip

## I2S Standard Protocol
The INMP441 uses a standard I2S protocol with 3 lines:
- **BCK** (Bit Clock): driven by the master (ESP32), 1 data bit transferred per BCK cycle.
- **WS** (Word Select): driven by the master. Low = left channel, High = right channel.
- **DATA**: driven by the slave (microphone), audio bits outputted MSB first.

### Timing diagram
```
BCK   _|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_|‾|_
WS    ‾‾‾‾‾‾‾‾‾|_________________________|‾‾‾‾‾‾‾‾‾‾‾‾
DATA  ----X delay X  B31  X  B30  X ... X  B0  X------
```

1. WS changes on the falling edge of BCK.
2. There's a 1-bit delay slot: MSB (B31) appears one BCK cycle after WS changes, not immediately.
3. Data is output MSB first and held stable through the rising edge of BCK (where the master samples it).

---

## INMP441 Output
The INMP441 captures 24-bit audio samples and outputs them left-justified in a 32-bit word:
```
Bit 31 (MSB) ... Bit 8 : 24-bit audio sample
Bit 7        ... Bit 0 : always 0
```

When reading with ESP-IDF using `I2S_BITS_PER_SAMPLE_32BIT`, shift the result right by 8 bits to get the 24-bit value, or right by 16 bits to get a 16-bit value for most audio processing.

```c
int32_t raw    = buffer[i];
int32_t s24    = raw >> 8;   // 24-bit signed sample
int16_t s16    = raw >> 16;  // 16-bit signed sample (loses precision)
```

---

## Custom Chip Implementation
The chip implementation (`src/main.c`) watches BCK using Wokwi's `pin_watch` API and outputs bits on the DATA pin on each falling edge, matching the INMP441 timing:
1. On each falling BCK edge, check whether WS changed.
2. If WS changed: load the next sample into `current_sample`, set `bit_count = -1` (delay slot).
3. If `bit_count == -1`: output 0 on DATA (delay slot), advance to `bit_count = 0`.
4. Otherwise: output `(current_sample >> (31 - bit_count)) & 1` and increment `bit_count`.
5. After bit 31: output 0 for any remaining clocks in the word.

---

## ESP-IDF Configuration
```c
i2s_config_t i2s_config = {
  .mode = I2S_MODE_MASTER | I2S_MODE_RX,
  .sample_rate = 16000,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .dma_buf_count = 8,
  .dma_buf_len = 64,
};
```

`I2S_CHANNEL_FMT_ONLY_LEFT` combined with L/R tied to GND on the INMP441 means only left-channel words are captured. WS still toggles; the right-channel words are discarded by the driver.