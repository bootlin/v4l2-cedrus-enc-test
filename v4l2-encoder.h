/*
 * Copyright (C) 2019-2023 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2020-2023 Bootlin
 */

#ifndef _V4L2_ENCODER_H_
#define _V4L2_ENCODER_H_

#include <linux/videodev2.h>

#include <draw.h>

struct v4l2_encoder;

struct v4l2_encoder_buffer {
	struct v4l2_encoder *encoder;

	struct v4l2_buffer buffer;
	struct v4l2_plane planes[4];
	unsigned int planes_count;

	void *mmap_data[4];
	bool queued;
	int request_fd;
};

struct v4l2_encoder_setup {
	/* Dimensions */
	unsigned int width;
	unsigned int height;

	/* Format */
	uint32_t format;

	/* Framerate */
	unsigned int fps_num;
	unsigned int fps_den;

	/* Quality */
	unsigned int qp_i;
	unsigned int qp_p;

	unsigned int gop_closure;
	unsigned int gop_size;
};

struct v4l2_encoder {
	int video_fd;
	int media_fd;

	char driver[32];
	char card[32];

	unsigned int capabilities;
	unsigned int memory;

	bool up;
	bool started;

	struct v4l2_encoder_setup setup;

	unsigned int output_type;
	unsigned int output_capabilities;
	struct v4l2_format output_format;
	struct v4l2_encoder_buffer output_buffers[3];
	unsigned int output_buffers_count;
	unsigned int output_buffers_index;

	unsigned int capture_type;
	unsigned int capture_capabilities;
	struct v4l2_format capture_format;
	struct v4l2_encoder_buffer capture_buffers[3];
	unsigned int capture_buffers_count;
	unsigned int capture_buffers_index;
	unsigned int capture_returned_index;

	unsigned int frame_number;

	struct draw_mandelbrot draw_mandelbrot;
	struct draw_buffer *draw_buffer;
	unsigned int pattern_step;

	unsigned int x, y;
	bool pattern_drawn;
	bool direction;

	int bitstream_fd;
};

int v4l2_encoder_prepare(struct v4l2_encoder *encoder);
int v4l2_encoder_complete(struct v4l2_encoder *encoder);
int v4l2_encoder_run(struct v4l2_encoder *encoder);
int v4l2_encoder_start(struct v4l2_encoder *encoder);
int v4l2_encoder_stop(struct v4l2_encoder *encoder);
int v4l2_encoder_buffer_setup(struct v4l2_encoder_buffer *buffer,
			     unsigned int type, unsigned int index);
int v4l2_encoder_buffer_cleanup(struct v4l2_encoder_buffer *buffer);
int v4l2_encoder_setup_defaults(struct v4l2_encoder *encoder);
int v4l2_encoder_setup_dimensions(struct v4l2_encoder *encoder,
				  unsigned int width, unsigned int height);
int v4l2_encoder_setup_format(struct v4l2_encoder *encoder, uint32_t format);
int v4l2_encoder_setup_fps(struct v4l2_encoder *encoder, float fps);
int v4l2_encoder_setup_qp(struct v4l2_encoder *encoder, unsigned int qp_i,
			  unsigned int qp_p);
int v4l2_encoder_setup_gop(struct v4l2_encoder *encoder, unsigned int closure,
			   unsigned int size);

int v4l2_encoder_setup(struct v4l2_encoder *encoder);
int v4l2_encoder_cleanup(struct v4l2_encoder *encoder);
int v4l2_encoder_probe(struct v4l2_encoder *encoder);
int v4l2_encoder_open(struct v4l2_encoder *encoder);
void v4l2_encoder_close(struct v4l2_encoder *encoder);

#endif
