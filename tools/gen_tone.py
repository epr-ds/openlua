#!/usr/bin/env python3
"""Generate a test tone for the OpenLua sound demo.

Usage:  python3 tools/gen_tone.py [path]   (default: assets/beep.wav)
Writes a 2 s, 440 Hz sine, 44100 Hz, 16-bit mono WAV at ~60% amplitude.
On the PSP it is resampled to 22050 Hz on load.
"""
import os, struct, math, sys

def write_wav(path: str, samples, rate: int) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    data = b"".join(struct.pack("<h", max(-32768, min(32767, s))) for s in samples)
    header = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16)
    header += b"data" + struct.pack("<I", len(data))
    with open(path, "wb") as f:
        f.write(header + data)

def main() -> None:
    path = sys.argv[1] if len(sys.argv) > 1 else "assets/beep.wav"
    rate = 44100
    n = rate * 2
    amp = int(2**15 * 0.6)
    samples = [int(amp * math.sin(2 * math.pi * 440 * t / rate)) for t in range(n)]
    write_wav(path, samples, rate)
    print(f"wrote {path}: 440 Hz sine, {len(samples)} samples @ {rate} Hz (2 s mono WAV)")

if __name__ == "__main__":
    main()
