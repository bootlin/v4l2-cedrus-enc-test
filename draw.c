/*
 * Copyright (C) 2020 Bootlin
 */

#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include <string.h>
#include <math.h>
#include <cairo.h>

#include <draw.h>
#include <csc.h>

struct draw_buffer *draw_buffer_create(unsigned int width, unsigned int height)
{
	struct draw_buffer *buffer = NULL;
	unsigned int stride;
	unsigned int size;

	if (!width || !height)
		return NULL;

	buffer = calloc(1, sizeof(*buffer));
	if (!buffer)
		goto error;

	stride = width * 4;
	size = stride * height;

	buffer->width = width;
	buffer->height = height;
	buffer->stride = stride;

	buffer->size = size;
	buffer->data = calloc(1, size);

	if (!buffer->data)
		goto error;

	return buffer;

error:
	if (buffer)
		free(buffer);

	return NULL;
}

void draw_buffer_destroy(struct draw_buffer *buffer)
{
	if (!buffer)
		return;

	if (buffer->data)
		free(buffer->data);

	free(buffer);
}

void draw_png(struct draw_buffer *buffer, char *path)
{
	cairo_surface_t *surface = NULL;
	unsigned int width;
	unsigned int height;
	unsigned int stride;
	unsigned int y;
	void *data;

	surface = cairo_image_surface_create_from_png(path);
	if (!surface)
		return;

	data = cairo_image_surface_get_data(surface);
	width = cairo_image_surface_get_width(surface);
	height = cairo_image_surface_get_height(surface);
	stride = cairo_image_surface_get_stride(surface);

	for (y = 0; y < height; y++)
		memcpy(buffer->data + y * buffer->stride,
		       data + y * stride, width * sizeof(uint32_t));

	cairo_surface_destroy(surface);
}

void draw_background(struct draw_buffer *buffer, uint32_t color)
{
	wmemset(buffer->data, color, buffer->size / sizeof(color));
}

void draw_gradient(struct draw_buffer *buffer)
{
	unsigned int x, y;
	uint32_t *pixel;

	for (y = 0; y < buffer->height; y++) {
		for (x = 0; x < buffer->width; x++) {
			unsigned int red = 255 * x / (buffer->width - 1);
			unsigned int blue = 255 * y / (buffer->height - 1);
			uint32_t value = 0x00000000;

			value |= (red & 0xff) << 0;
			value |= (blue & 0xff) << 8;

			pixel = draw_buffer_pixel(buffer, x, y);
			*pixel = value;
		}
	}
}

void draw_rectangle(struct draw_buffer *buffer, unsigned int x_start,
		    unsigned int y_start, unsigned int width,
		    unsigned int height, uint32_t color)
{
	unsigned int x_stop = x_start + width;
	unsigned int y_stop = y_start + height;
	unsigned int x, y;
	uint32_t *pixel;

	for (y = y_start; y < y_stop; y++) {
		pixel = draw_buffer_pixel(buffer, x_start, y);
		wmemset(pixel, color, width);
	}
}

struct nv12_color {
	unsigned char y;
	unsigned char u;
	unsigned char v;
};

static struct nv12_color colors[] = {
	{ 104, 128, 128 },	/* 40% gray */
	{ 180, 128, 128 },	/* 75% white */
	{ 168, 44, 136 },	/* 75% cyan */
	{ 133, 63, 52 },	/* 75% green */
	{ 63, 193, 204 },	/* 75% magenta */
	{ 51, 109, 212 },	/* 75% red */
	{ 28, 212, 120 },	/* 75% blue */
	{ 16, 128, 128 },	/* 75% black */
};

static unsigned int colors_count = sizeof(colors) / sizeof(*colors);

void test_pattern_step(unsigned int width, unsigned int height, unsigned int stride, unsigned int step, void *luma, void *chroma)
{
	unsigned int box_height = 50;
	unsigned int box_y = ((step * 2) % (height - box_height));
	unsigned int x, y, i;
	unsigned char *l, *c;
	unsigned int color_width = width / colors_count;

	l = luma;
	c = chroma;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			unsigned int index = x / color_width;
			struct nv12_color color = colors[index];

			if (y >= box_y && y < (box_y + box_height)) {
				color.y = 255 - color.y;
				color.u = 255 - color.u;
				color.v = 255 - color.v;
			}

			*l++ = color.y;

			/* YUV 420 */
			if (!(y % 2) && !(x % 2)) {
				*c++ = color.u;
				*c++ = color.v;
			}

			/* YUV 422 */
/*
			if (!(x % 2)) {
				*c++ = color.u;
				*c++ = color.v;
			}
*/
		}

		if (stride - width > 0) {
			l += stride - width;
			c += stride - width;
		}
	}
}

struct rgb_color {
	unsigned int r;
	unsigned int g;
	unsigned int b;
};

static struct rgb_color mandelbrot_colors[] = {
	{ 66, 30, 15, },
	{ 25, 7, 26, },
	{ 9, 1, 47, },
	{ 4, 4, 73, },
	{ 0, 7, 100, },
	{ 12, 44, 138, },
	{ 24, 82, 177, },
	{ 57, 125, 209, },
	{ 134, 181, 229, },
	{ 211, 236, 248, },
	{ 241, 233, 191, },
	{ 248, 201, 95, },
	{ 255, 170, 0, },
	{ 204, 128, 0, },
	{ 153, 87, 0, },
	{ 106, 52, 3, },
};

unsigned int mandelbrot_colors_count =
	sizeof(mandelbrot_colors) / sizeof(mandelbrot_colors[0]);

void draw_mandelbrot(struct draw_mandelbrot *mandelbrot,
		     struct draw_buffer *buffer)
{
	unsigned int *pixel;
	unsigned int x, y;
	unsigned int width;
	unsigned int height;
	unsigned int leftover;
	float diff_x;
	float diff_y;
	float fact_x;
	float fact_y;
	float start_x;
	float start_y;
	unsigned int iterations;
	float scale_iter;
	float scale_depth = 255.;

	if (!mandelbrot)
		return;

	pixel = buffer->data;

	width = buffer->width;
	height = buffer->height;
	leftover = (buffer->stride / sizeof(*pixel) - buffer->width);

	diff_x = mandelbrot->bounds_x[1] - mandelbrot->bounds_x[0];
	diff_y = mandelbrot->bounds_y[1] - mandelbrot->bounds_y[0];
	fact_x = diff_x / buffer->width;
	fact_y = diff_x / buffer->height;
	start_x = mandelbrot->bounds_x[0];
	start_y = mandelbrot->bounds_y[0];
	iterations = mandelbrot->iterations;
	scale_iter = 1.0f / iterations;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			float cr = x * fact_x + start_x;
			float ci = y * fact_y + start_y;
			float zr = cr;
			float zi = ci;
			float mkr = 0.0f;
			float mkg = 0.0f;
			float mkb = 0.0f;
			unsigned int k = 0;
			struct rgb_color *color;
			unsigned int index;

			while (++k < iterations) {
				float zr_k = zr * zr - zi * zi + cr;
				float zi_k = zr * zi + zr * zi + ci;
				zr = zr_k;
				zi = zi_k;

				if (zr * zr + zi * zi >= 4.0f)
					break;
			}

			index = k % mandelbrot_colors_count;
			color = &mandelbrot_colors[index];

			*pixel = rgb_pixel(color->r, color->g, color->b);
			pixel++;
       		}

		pixel += leftover;
	}
}

void draw_mandelbrot_zoom(struct draw_mandelbrot *mandelbrot)
{
	if (!mandelbrot)
		return;

	mandelbrot->view_width /= mandelbrot->zoom;
	mandelbrot->view_height /= mandelbrot->zoom;

	mandelbrot->iterations_zoom += sqrtf(mandelbrot->zoom) / 2.;
	mandelbrot->iterations = (unsigned int)mandelbrot->iterations_zoom;

	mandelbrot->bounds_x[0] = mandelbrot->center_x - mandelbrot->view_width / 2.;
	mandelbrot->bounds_x[1] = mandelbrot->center_x + mandelbrot->view_width / 2.;

	mandelbrot->bounds_y[0] = mandelbrot->center_y - mandelbrot->view_height / 2.;
	mandelbrot->bounds_y[1] = mandelbrot->center_y + mandelbrot->view_height / 2.;
}

void draw_mandelbrot_init(struct draw_mandelbrot *mandelbrot)
{
	if (!mandelbrot)
		return;

	mandelbrot->zoom = 1.02;
	mandelbrot->center_x = -0.743643887037151;
	mandelbrot->center_y = 0.13182590420533;
	mandelbrot->view_width = 0.005671;
	mandelbrot->view_height = mandelbrot->view_width * 720. / 1280.;
	mandelbrot->iterations_zoom = 200.;
}
