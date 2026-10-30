#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 the freeciv-3ds authors
"""Generate the UI click (3ds_port/romfs/audio/sfx/click.wav): a short, quiet synthetic tick.

usage: make_click.py OUT.wav   (16-bit PCM; tools/make_audio.sh converts it to IMA ADPCM)
"""
import math
import random
import struct
import sys
import wave

RATE, LENGTH, PEAK = 22050, 2041, 12500   # 93 ms; peak about -9 dBFS, decays in ~40 ms
rng = random.Random(3)                     # fixed seed: same file every run
samples = []
for i in range(LENGTH):
    env = math.exp(-i / 140.0)
    tone = math.sin(2 * math.pi * 1350 * i / RATE)
    noise = rng.uniform(-1, 1)
    samples.append(int(PEAK * env * (0.8 * tone + 0.2 * noise)))
with wave.open(sys.argv[1], 'wb') as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(RATE)
    w.writeframes(struct.pack('<%dh' % LENGTH, *samples))
