// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 the freeciv-3ds authors
// World geometry of the isometric (Amplio2) map, in native coordinates.
// Freeciv's ISO topology staggers native rows: row y is shifted half a tile
// right when y is odd, and rows are half a tile apart. One world pixel is one
// pixel of the half-size Amplio2 sprites at zoom 1.0.
#pragma once
#ifndef ISO_W
#define ISO_W 48
#define ISO_H 24
#endif
#define ISO_ROW (ISO_H / 2)

static inline int iso_wx(int nx, int ny) { return nx * ISO_W + (ny & 1) * (ISO_W / 2); }
static inline int iso_wy(int ny) { return ny * ISO_ROW; }
