/*
 * Copyright (C) 2020 Bootlin
 */

#ifndef _CSC_H_
#define _CSC_H_

static inline uint8_t byte_range(float v)
{
	if (v < 0.)
		return 0;
	else if (v > 255.)
		return 255;
	else
		return (uint8_t)v;
}

int rgb2yuv420(struct draw_buffer *buffer, void *buffer_y, void *buffer_u,
	       void *buffer_v);
int rgb2nv12(struct draw_buffer *buffer, void *buffer_y, void *buffer_uv);
unsigned int rgb_pixel(unsigned int r, unsigned int g, unsigned int b);
unsigned int hsv2rgb_pixel(float hi, float si, float vi);

#endif
