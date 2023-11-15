/*
 * Copyright (C) 2020 Bootlin
 */

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>

#include <draw.h>
#include <csc.h>

int rgb2yuv420(struct draw_buffer *buffer, void *buffer_y, void *buffer_u,
	       void *buffer_v)
{
	unsigned int width, height, stride;
	unsigned int x, y;
	void *data;
	
	if (!buffer)
		return -EINVAL;

	data = buffer->data;
	width = buffer->width;
	height = buffer->height;
	stride = buffer->stride;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			uint8_t *crgb;
			uint8_t *cy;
			float value;

			crgb = data + stride * y + x * 4;
			value = crgb[2] * 0.299 + crgb[1] * 0.587 + crgb[0] * 0.114;

			cy = buffer_y + width * y + x;
			*cy = byte_range(value);

			if ((x % 2) == 0 && (y % 2) == 0) {
				uint8_t *cu;
				uint8_t *cv;

				value = crgb[2] * -0.14713 + crgb[1] * -0.28886 + crgb[0] * 0.436 + 128.;

				cu = buffer_u + width / 2 * y / 2 + x / 2;
				*cu = byte_range(value);

				value = crgb[2] * 0.615 + crgb[1] * -0.51499 + crgb[0] * -0.10001 + 128.;

				cv = buffer_v + width / 2 * y / 2 + x / 2;
				*cv = byte_range(value);
			}
		}
	}

	return 0;
}

int rgb2nv12(struct draw_buffer *buffer, void *buffer_y, void *buffer_uv)
{
	unsigned int width, height, stride;
	unsigned int x, y;
	void *data;

	if (!buffer)
		return -EINVAL;

	data = buffer->data;
	width = buffer->width;
	height = buffer->height;
	stride = buffer->stride;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			uint8_t *crgb;
			uint8_t *cy;
			float value;

			crgb = data + stride * y + x * 4;
			value = crgb[2] * 0.299 + crgb[1] * 0.587 + crgb[0] * 0.114;

			cy = buffer_y + width * y + x;
			*cy = byte_range(value);

			if ((x % 2) == 0 && (y % 2) == 0) {
				uint8_t *cu;
				uint8_t *cv;

				value = crgb[2] * -0.14713 + crgb[1] * -0.28886 + crgb[0] * 0.436 + 128.;

				cu = buffer_uv + width * y / 2 + x;
				*cu = byte_range(value);

				value = crgb[2] * 0.615 + crgb[1] * -0.51499 + crgb[0] * -0.10001 + 128.;

				cv = buffer_uv + width * y / 2 + x + 1;
				*cv = byte_range(value);
			}
		}
	}

	return 0;
}

unsigned int rgb_pixel(unsigned int r, unsigned int g, unsigned int b)
{
	return (255 << 24) | (r << 16) | (g << 8) | (b << 0);
}

unsigned int hsv2rgb_pixel(float hi, float si, float vi)
{
	unsigned int pixel;
	unsigned int ro, go, bo;
	float r, g, b;

	float h = hi / 360.0;
	float s = si / 100.0;
	float v = vi / 100.0;
	
	int i = floor(h * 6.0);
	float f = h * 6 - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);
	
	switch (i % 6) {
		case 0: r = v, g = t, b = p; break;
		case 1: r = q, g = v, b = p; break;
		case 2: r = p, g = v, b = t; break;
		case 3: r = p, g = q, b = v; break;
		case 4: r = t, g = p, b = v; break;
		case 5: r = v, g = p, b = q; break;
	}

	ro = (unsigned int)(r * 255.0);
	if (ro > 255)
		ro = 255;

	go = (unsigned int)(g * 255.0);
	if (go > 255)
		go = 255;

	bo = (unsigned int)(b * 255.0);
	if (bo > 255)
		bo = 255;

	return rgb_pixel(ro, go, bo);
}
