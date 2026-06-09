"""
Receive audio from ESP32, save as WAV file, visualize, and play back.

"""
import argparse
import struct
import wave
import time

import serial
import numpy as np
import matplotlib.pyplot as plt
import sounddevice as sd
from scipy import signal as scipy_signal

SAMPLE_RATE = 8000
BAUD_RATE = 230400
SYNC_HEADER = bytes([0xAA, 0xBB, 0xCC, 0xDD])

def find_sync(ser: serial.Serial) -> bool:
    buf = bytearray()
    deadline = time.time() + 3.0
    while time.time() < deadline:
        b = ser.read(1)
        if not b:
            continue
        buf.append(b[0])
        if len(buf) > 4:
            buf.pop(0)
        if bytes(buf) == SYNC_HEADER:
            return True
    return False

def receive(port: str, duration: float) -> np.ndarray:
    n_samples = int(SAMPLE_RATE * duration)
    total_bytes = n_samples * 2  # int16 = 2 bytes per sample

    print(f"Opening {port} at {BAUD_RATE} baud...")
    with serial.Serial(port, BAUD_RATE, timeout=5) as ser:
        ser.reset_input_buffer()
        print(f"Recording {duration}s ... speak now!")

        data = bytearray()
        while len(data) < total_bytes:
            chunk = ser.read(min(2048, total_bytes - len(data)))
            if chunk:
                data.extend(chunk)
            pct = len(data) / total_bytes * 100
            print(f"  {len(data)//2}/{n_samples} samples ({pct:.0f}%)", end='\r')

    print(f"\nDone.")
    # if odd number of bytes arrived, drop the last one
    if len(data) % 2 != 0:
        data = data[:-1]
    return np.frombuffer(data, dtype='<i2').copy()
    print(f"\nDone. Received {received} samples.")
    return samples

def save_wav(samples: np.ndarray, path: str):
    # Remove DC offset and normalize to full 16-bit range
    audio = samples.astype(np.float32)
    audio -= audio.mean() # remove DC spike
    peak = np.abs(audio).max()
    if peak > 0:
        audio = audio / peak * 32000 # normalize to near-full scale
    normalized = audio.astype(np.int16)

    with wave.open(path, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(normalized.tobytes())
    print(f"Saved to {path}")
    return normalized # return for visualization

def visualize(samples: np.ndarray, path: str):
    # Remove DC and normalize for display
    audio = samples.astype(np.float32)
    audio -= audio.mean()
    peak = np.abs(audio).max()
    if peak > 0:
        audio = audio / peak

    t = np.arange(len(audio)) / SAMPLE_RATE

    fig, axes = plt.subplots(2, 1, figsize=(12, 6))
    fig.suptitle("INMP441 Recording")

    axes[0].plot(t, audio, linewidth=0.5, color='steelblue')
    axes[0].set_xlabel("Time (s)")
    axes[0].set_ylabel("Normalized amplitude")
    axes[0].set_ylim(-1.1, 1.1)
    axes[0].set_title("Waveform")
    axes[0].grid(True, alpha=0.3)

    f, t_spec, Sxx = scipy_signal.spectrogram(audio, fs=SAMPLE_RATE, nperseg=512, noverlap=256)
    Sxx_db = 10 * np.log10(np.maximum(Sxx, 1e-10))  # floor prevents log10(0)
    axes[1].pcolormesh(t_spec, f, Sxx_db, cmap='inferno', shading='gouraud')

    axes[1].set_xlabel("Time (s)")
    axes[1].set_ylabel("Frequency (Hz)")
    axes[1].set_title("Spectrogram")

    plt.tight_layout()
    img_path = path.replace('.wav', '.png')
    plt.savefig(img_path, dpi=150)
    print(f"Plot saved to {img_path}")
    plt.show()

def play(samples: np.ndarray):
    print("Playing back audio...")
    audio = samples.astype(np.float32)
    audio -= audio.mean()
    peak = np.abs(audio).max()
    if peak > 0:
        audio = audio / peak * 0.9
    sd.play(audio, SAMPLE_RATE)
    sd.wait()
    print("Playback done.")

def main():
    parser = argparse.ArgumentParser(description="Receive audio from ESP32 over serial")
    parser.add_argument('--port',     default='COM3',          help='Serial port (default: COM3)')
    parser.add_argument('--duration', default=3.0, type=float, help='Recording duration in seconds (default: 3)')
    parser.add_argument('--out',      default='recording.wav', help='Output WAV file (default: recording.wav)')
    parser.add_argument('--no-play',  action='store_true',     help='Skip audio playback')
    args = parser.parse_args()

    samples = receive(args.port, args.duration)
    save_wav(samples, args.out) # returns normalized (don't need it for visualize)
    visualize(samples, args.out) # visualize handles its own normalization
    
    if not args.no_play:
        play(samples)

if __name__ == '__main__':
    main()