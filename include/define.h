#pragma once

#define WIDTH 320
#define HEIGHT 240
#define PIX_LEN WIDTH*HEIGHT

#define ScanOrder(i) (i/64)*64 + scan_order[i%64]
#define MAX(a,b)  (((a)>(b))?(a):(b))
#define MIN(a,b)  (((a)<(b))?(a):(b))
#define CLIP(n, min, max) MIN((MAX((n),(min))), (max))
