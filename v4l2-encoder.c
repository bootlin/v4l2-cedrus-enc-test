/*
 * Copyright (C) 2019-2023 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2020-2023 Bootlin
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <libudev.h>

#include <linux/videodev2.h>
#include <linux/media.h>

#include <media.h>
#include <v4l2.h>
#include <v4l2-encoder.h>
#include <csc.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

int v4l2_encoder_complete(struct v4l2_encoder *encoder)
{
	struct v4l2_encoder_buffer *buffer;
	unsigned int index;
	unsigned int length;
	char frame_type;
	int ret;

	if (!encoder)
		return -EINVAL;

	index = encoder->capture_returned_index;
	buffer = &encoder->capture_buffers[index];


	if (buffer->buffer.flags & V4L2_BUF_FLAG_KEYFRAME)
		frame_type = 'I';
	else if (buffer->buffer.flags & V4L2_BUF_FLAG_PFRAME)
		frame_type = 'P';
	else
		frame_type = '?';

	v4l2_buffer_plane_length_used(&buffer->buffer, 0, &length);

	if (buffer->buffer.flags & V4L2_BUF_FLAG_ERROR)
		printf("Error encoding frame\n");
	else
		printf("Encoded %c frame in %u bytes\n", frame_type, length);

	if (encoder->bitstream_fd >= 0 && length > 0)
		write(encoder->bitstream_fd, buffer->mmap_data[0], length);

	encoder->frame_number++;

	return 0;
}

int v4l2_encoder_prepare(struct v4l2_encoder *encoder)
{
	struct v4l2_encoder_buffer *output_buffer;
	unsigned int output_index;
	unsigned int width, height;
	const unsigned int pattern_step = 0;
	int pixelformat;
	int fd;
	int ret;

	if (!encoder)
		return -EINVAL;

	v4l2_format_pixel(&encoder->output_format, &width, &height,
			  &pixelformat);

	output_index = encoder->output_buffers_index;
	output_buffer = &encoder->output_buffers[output_index];

#define PATTERN

#ifdef MANDELBROT
#define CONVERT_RGB_NV12
	draw_mandelbrot_zoom(&encoder->draw_mandelbrot);
	draw_mandelbrot(&encoder->draw_mandelbrot, encoder->draw_buffer);
#endif
#ifdef GRADIENT
#define CONVERT_RGB_NV12
	draw_gradient(encoder->draw_buffer);
#endif
#ifdef RECTANGLE
#define CONVERT_RGB_NV12
	draw_background(encoder->draw_buffer, 0xff00ffff);
	draw_rectangle(encoder->draw_buffer, encoder->x, height / 3, width / 3,
		       height / 3, 0x00ff0000);

	if (!encoder->direction) {
		if (encoder->x >= 20) {
			encoder->x -= 20;
		} else {
			encoder->x = 0;
			encoder->direction = 1;
		}
	} else {
		if (encoder->x < (2 * width / 3 - 20)) {
			encoder->x += 20;
		} else {
			encoder->x = 2 * width / 3;
			encoder->direction = 0;
		}
	}
#endif
#ifdef PATTERN_PNG
#define CONVERT_RGB_NV12
	if (!encoder->pattern_drawn) {
		draw_png(encoder->draw_buffer, "test-pattern.png");
		encoder->pattern_drawn = true;
	}
#endif
#ifdef PATTERN
	/* XXX: fixup stride. */
	test_pattern_step(width, height, width, encoder->pattern_step, output_buffer->mmap_data[0], output_buffer->mmap_data[0] + width * height);

	encoder->pattern_step++;
#endif

	printf("Drawing done\n");

#ifdef CONVERT_RGB_NV12
	if (pixelformat == V4L2_PIX_FMT_YUV420M)
		ret = rgb2yuv420(encoder->draw_buffer,
				 output_buffer->mmap_data[0],
				 output_buffer->mmap_data[1],
				 output_buffer->mmap_data[2]);
	else if (pixelformat == V4L2_PIX_FMT_NV12M)
		ret = rgb2nv12(encoder->draw_buffer,
			       output_buffer->mmap_data[0],
			       output_buffer->mmap_data[1]);
	else if (pixelformat == V4L2_PIX_FMT_NV12)
		ret = rgb2nv12(encoder->draw_buffer,
			       output_buffer->mmap_data[0],
			       output_buffer->mmap_data[0] + width * height);
#endif

#ifdef OUTPUT_DUMP
	fd = open("output.yuv",  O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		printf("output open error!\n");
		return -1;
	}

	write(fd, output_buffer->mmap_data[0], width * height);
	write(fd, output_buffer->mmap_data[1], width * height / 4);
	write(fd, output_buffer->mmap_data[2], width * height / 4);

	close(fd);
#endif

	return 0;
}

#define timespec_diff(tb, ta) \
       ((ta.tv_sec * 1000000000UL + ta.tv_nsec) - (tb.tv_sec * 1000000000UL + tb.tv_nsec))

int v4l2_encoder_run(struct v4l2_encoder *encoder)
{
	struct v4l2_encoder_buffer *output_buffer;
	unsigned int output_index;
	struct v4l2_encoder_buffer *capture_buffer;
	unsigned int capture_index;
	struct v4l2_buffer buffer;
	unsigned int frame_number = encoder->frame_number;
	struct timespec time_before, time_after;
	uint64_t time_diff;
	uint64_t timestamp;
	struct timeval timeout = { 0, 300000 };
	unsigned int length = 0;
	bool force_key_frame = false;
	int ret;

	if (!encoder)
		return -EINVAL;

	if (force_key_frame) {
		ret = v4l2_encoder_control_set(encoder,
					       V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME,
					       0);
		if (ret)
			return ret;
	}

	output_index = encoder->output_buffers_index;
	output_buffer = &encoder->output_buffers[output_index];

	encoder->output_buffers_index++;
	encoder->output_buffers_index %= encoder->output_buffers_count;

	v4l2_buffer_plane_length(&output_buffer->buffer, 0, &length);
	v4l2_buffer_setup_plane_length_used(&output_buffer->buffer, 0, length);

	v4l2_buffer_setup_timestamp(&output_buffer->buffer,
				    frame_number * 1000UL);

	printf("Queue picture frame %u in buffer %u\n", frame_number,
	       output_index);

	ret = v4l2_buffer_queue(encoder->video_fd, &output_buffer->buffer);
	if (ret)
		return ret;

	capture_index = encoder->capture_buffers_index;
	capture_buffer = &encoder->capture_buffers[capture_index];

	encoder->capture_buffers_index++;
	encoder->capture_buffers_index %= encoder->capture_buffers_count;

	printf("Queue coded buffer %u\n", capture_index);

	clock_gettime(CLOCK_MONOTONIC, &time_before);

	ret = v4l2_buffer_queue(encoder->video_fd, &capture_buffer->buffer);
	if (ret)
		return ret;


	ret = v4l2_poll(encoder->video_fd, &timeout);
	if (ret <= 0)
		return ret;

	clock_gettime(CLOCK_MONOTONIC, &time_after);

	v4l2_buffer_setup_base(&buffer, encoder->output_type, encoder->memory);

	do {
		ret = v4l2_buffer_dequeue(encoder->video_fd, &buffer);
		if (ret && ret != -EAGAIN)
			return ret;
	} while (ret == -EAGAIN);

	printf("Dequeue picture buffer %u\n", buffer.index);

	if (buffer.index != output_buffer->buffer.index) {
		printf("Picture index mismatch!\n");
		return -1;
	}

	v4l2_buffer_setup_base(&buffer, encoder->capture_type, encoder->memory);

	do {
		ret = v4l2_buffer_dequeue(encoder->video_fd, &buffer);
		if (ret && ret != -EAGAIN)
			return ret;
	} while (ret == -EAGAIN);

	v4l2_buffer_timestamp(&buffer, &timestamp);
	frame_number = timestamp / 1000UL;

	printf("Dequeue coded frame %u in buffer %u\n", frame_number,
	       capture_index);

	if (buffer.index != capture_buffer->buffer.index) {
		printf("Picture index mismatch!\n");
		return -1;
	}

	encoder->capture_returned_index = buffer.index;
	encoder->capture_buffers[buffer.index].buffer = buffer;

	time_diff = timespec_diff(time_before, time_after);

	printf("Encode run took %llu us\n", time_diff / 1000ULL);

	return 0;
}

int v4l2_encoder_start(struct v4l2_encoder *encoder)
{
	int ret;

	if (!encoder || encoder->started)
		return -EINVAL;

	ret = v4l2_stream_on(encoder->video_fd, encoder->output_type);
	if (ret)
		return ret;

	ret = v4l2_stream_on(encoder->video_fd, encoder->capture_type);
	if (ret)
		return ret;

	encoder->started = true;

	return 0;
}

int v4l2_encoder_stop(struct v4l2_encoder *encoder)
{
	int ret;

	if (!encoder || !encoder->started)
		return -EINVAL;

	ret = v4l2_stream_off(encoder->video_fd, encoder->output_type);
	if (ret)
		return ret;

	ret = v4l2_stream_off(encoder->video_fd, encoder->capture_type);
	if (ret)
		return ret;

	encoder->started = false;

	return 0;
}

int v4l2_encoder_buffer_setup(struct v4l2_encoder_buffer *buffer,
			     unsigned int type, unsigned int index)
{
	struct v4l2_encoder *encoder;
	int ret;

	if (!buffer || !buffer->encoder)
		return -EINVAL;

	encoder = buffer->encoder;

	v4l2_buffer_setup_base(&buffer->buffer, type, encoder->memory);
	v4l2_buffer_setup_index(&buffer->buffer, index);
	v4l2_buffer_setup_planes(&buffer->buffer, type, buffer->planes,
				 buffer->planes_count);

	ret = v4l2_buffer_query(encoder->video_fd, &buffer->buffer);
	if (ret) {
		fprintf(stderr, "Failed to query buffer\n");
		goto complete;
	}

	if(encoder->memory == V4L2_MEMORY_MMAP) {
		unsigned int i;

		for (i = 0; i < buffer->planes_count; i++) {
			unsigned int offset;
			unsigned int length;

			ret = v4l2_buffer_plane_offset(&buffer->buffer, i,
						       &offset);
			if (ret)
				goto complete;

			ret = v4l2_buffer_plane_length(&buffer->buffer, i,
						       &length);
			if (ret)
				goto complete;

			buffer->mmap_data[i] =
				mmap(NULL, length, PROT_READ | PROT_WRITE,
				     MAP_SHARED, encoder->video_fd, offset);
			if (buffer->mmap_data[i] == MAP_FAILED) {
				ret = -errno;
				goto complete;
			}

			printf("mapped plane %d length %d\n", i, length);
		}
	}

	if (type == encoder->output_type) {
		buffer->request_fd = media_request_alloc(encoder->media_fd);
		if (buffer->request_fd < 0) {
			ret = -EINVAL;
			goto complete;
		}
	} else {
		buffer->request_fd = -1;
	}

	ret = 0;

complete:
	return ret;
}

int v4l2_encoder_buffer_cleanup(struct v4l2_encoder_buffer *buffer)
{
	struct v4l2_encoder *encoder;

	if (!buffer || !buffer->encoder)
		return -EINVAL;

	encoder = buffer->encoder;

	if(encoder->memory == V4L2_MEMORY_MMAP) {
		unsigned int i;

		for (i = 0; i < buffer->planes_count; i++) {
			unsigned int length;

			if (!buffer->mmap_data[i] ||
			    buffer->mmap_data[i] == MAP_FAILED)
					continue;

			v4l2_buffer_plane_length(&buffer->buffer, i, &length);
			munmap(buffer->mmap_data[i], length);
		}
	}

	if (buffer->request_fd >= 0)
		close(buffer->request_fd);

	memset(buffer, 0, sizeof(*buffer));
	buffer->request_fd = -1;

	return 0;
}

int v4l2_encoder_control_set(struct v4l2_encoder *encoder, unsigned int id,
			     int value)
{
	struct v4l2_control control;
	int ret;

	v4l2_control_setup_base(&control, id);
	v4l2_control_setup_value(&control, value);

	ret = v4l2_control_set(encoder->video_fd, &control);
	if (ret)
		fprintf(stderr, "Failed to set control\n");

	return ret;
}

int v4l2_encoder_setup_defaults(struct v4l2_encoder *encoder)
{
	int ret;

	if (!encoder)
		return -EINVAL;

	if (encoder->up)
		return -EBUSY;

	ret = v4l2_encoder_setup_dimensions(encoder, 1280, 720);
	if (ret)
		return ret;

	ret = v4l2_encoder_setup_format(encoder, V4L2_PIX_FMT_NV12);
	if (ret)
		return ret;

	ret = v4l2_encoder_setup_fps(encoder, 25);
	if (ret)
		return ret;

	ret = v4l2_encoder_setup_qp(encoder, 24, 26);
	if (ret)
		return ret;

	ret = v4l2_encoder_setup_gop(encoder, 0, 3);
	if (ret)
		return ret;

	return 0;
}

int v4l2_encoder_setup_dimensions(struct v4l2_encoder *encoder,
				  unsigned int width, unsigned int height)
{
	if (!encoder || !width || !height)
		return -EINVAL;

	if (encoder->up)
		return -EBUSY;

	encoder->setup.width = width;
	encoder->setup.height = height;

	return 0;
}

int v4l2_encoder_setup_format(struct v4l2_encoder *encoder, uint32_t format)
{
	if (!encoder)
		return -EINVAL;

	if (encoder->up)
		return -EBUSY;

	encoder->setup.format = format;

	return 0;
}

int v4l2_encoder_setup_fps(struct v4l2_encoder *encoder, float fps)
{
	if (!encoder || !fps)
		return -EINVAL;

	encoder->setup.fps_den = 1000;
	encoder->setup.fps_num = fps * encoder->setup.fps_den;

	return 0;
}

int v4l2_encoder_setup_qp(struct v4l2_encoder *encoder, unsigned int qp_i,
			  unsigned int qp_p)
{
	if (!encoder || !qp_i || !qp_p)
		return -EINVAL;

	encoder->setup.qp_i = qp_i;
	encoder->setup.qp_p = qp_p;

	return 0;
}

int v4l2_encoder_setup_gop(struct v4l2_encoder *encoder, unsigned int closure,
			   unsigned int size)
{
	if (!encoder)
		return -EINVAL;

	encoder->setup.gop_closure = closure;
	encoder->setup.gop_size = size;

	return 0;
}

int v4l2_encoder_setup(struct v4l2_encoder *encoder)
{
	unsigned int width, height;
	unsigned int width_coded, height_coded;
	unsigned int buffers_count;
	unsigned int capture_size;
	struct v4l2_streamparm streamparm;
	uint32_t format;
	unsigned int i;
	int ret;

	if (!encoder || encoder->up)
		return -EINVAL;

	capture_size = 2 * 1024 * 1024;
	width = encoder->setup.width;
	height = encoder->setup.height;
	format = encoder->setup.format;

	/* Setup capture format. */

	v4l2_format_setup_base(&encoder->capture_format, encoder->capture_type);
	v4l2_format_setup_pixel(&encoder->capture_format, width, height,
				V4L2_PIX_FMT_H264);
	v4l2_format_setup_sizeimage(&encoder->capture_format, 0, capture_size);

	ret = v4l2_format_set(encoder->video_fd, &encoder->capture_format);
	if (ret) {
		fprintf(stderr, "Failed to set capture format\n");
		goto complete;
	}

	/* Setup output format. */

	v4l2_format_setup_base(&encoder->output_format, encoder->output_type);
	v4l2_format_setup_pixel(&encoder->output_format, width, height, format);

	ret = v4l2_format_set(encoder->video_fd, &encoder->output_format);
	if (ret) {
		fprintf(stderr, "Failed to set output format\n");
		goto complete;
	}

	ret = v4l2_format_get(encoder->video_fd, &encoder->output_format);
	if (ret) {
		fprintf(stderr, "Failed to get output format\n");
		goto complete;
	}

	v4l2_format_pixel(&encoder->output_format, &width_coded, &height_coded,
			  NULL);

	if (width_coded != width || height_coded != height) {
		struct v4l2_selection selection;

		v4l2_selection_setup_base(&selection, encoder->output_type,
					  V4L2_SEL_TGT_CROP);
		v4l2_selection_setup_rect(&selection, 0, 0, width, height);

		ret = v4l2_selection_set(encoder->video_fd, &selection);
		if (ret) {
			fprintf(stderr, "Failed to set output selection\n");
			goto complete;
		}
	}

	/* Allocate capture buffers. */

	buffers_count = ARRAY_SIZE(encoder->capture_buffers);

	ret = v4l2_buffers_request(encoder->video_fd, encoder->capture_type,
				   encoder->memory, buffers_count);
	if (ret) {
		fprintf(stderr, "Failed to allocate capture buffers\n");
		goto error;
	}

	for (i = 0; i < buffers_count; i++) {
		struct v4l2_encoder_buffer *buffer = &encoder->capture_buffers[i];

		buffer->encoder = encoder;
		buffer->planes_count =
			v4l2_format_planes_count(&encoder->capture_format);

		ret = v4l2_encoder_buffer_setup(buffer, encoder->capture_type, i);
		if (ret) {
			fprintf(stderr, "Failed to setup capture buffer\n");
			goto error;
		}
	}

	encoder->capture_buffers_count = buffers_count;

	/* Allocate output buffers */

	buffers_count = ARRAY_SIZE(encoder->output_buffers);

	ret = v4l2_buffers_request(encoder->video_fd, encoder->output_type,
				   encoder->memory, buffers_count);
	if (ret) {
		fprintf(stderr, "Failed to allocate output buffers\n");
		goto complete;
	}

	for (i = 0; i < buffers_count; i++) {
		struct v4l2_encoder_buffer *buffer = &encoder->output_buffers[i];

		buffer->encoder = encoder;
		buffer->planes_count =
			v4l2_format_planes_count(&encoder->output_format);

		ret = v4l2_encoder_buffer_setup(buffer, encoder->output_type, i);
		if (ret) {
			fprintf(stderr, "Failed to setup output buffer\n");
			goto error;
		}
	}

	encoder->output_buffers_count = buffers_count;

	/* Controls */

	ret = v4l2_encoder_control_set(encoder,
				       V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR,
				       1);
	if (ret)
		goto error;

	ret = v4l2_encoder_control_set(encoder,
				       V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP,
				       encoder->setup.qp_i);
	if (ret)
		goto error;

	ret = v4l2_encoder_control_set(encoder,
				       V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP,
				       encoder->setup.qp_p);
	if (ret)
		goto error;

	ret = v4l2_encoder_control_set(encoder,
				       V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE,
				       V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC);
	if (ret)
		goto error;

	ret = v4l2_encoder_control_set(encoder, V4L2_CID_MPEG_VIDEO_GOP_CLOSURE,
				       encoder->setup.gop_closure);
	if (ret)
		goto error;

	if (encoder->setup.gop_closure) {
		ret = v4l2_encoder_control_set(encoder,
					       V4L2_CID_MPEG_VIDEO_GOP_SIZE,
					       encoder->setup.gop_size);
		if (ret)
			goto error;
	} else {
		ret = v4l2_encoder_control_set(encoder,
					       V4L2_CID_MPEG_VIDEO_H264_I_PERIOD,
					       encoder->setup.gop_size);
		if (ret)
			goto error;
	}

	/* Parm */

	v4l2_parm_setup_base(&streamparm, encoder->output_type);
	streamparm.parm.output.timeperframe.numerator = encoder->setup.fps_num;
	streamparm.parm.output.timeperframe.denominator = encoder->setup.fps_den;

	ret = v4l2_parm_set(encoder->video_fd, &streamparm);
	if (ret) {
		fprintf(stderr, "Failed to set output parm\n");
		goto error;
	}

	/* Draw buffer */

	encoder->draw_buffer = draw_buffer_create(width, height);
	if (!encoder->draw_buffer) {
		fprintf(stderr, "Failed to create draw buffer\n");
		goto error;
	}

	/* Mandelbrot */

	draw_mandelbrot_init(&encoder->draw_mandelbrot);

	encoder->up = true;

	ret = 0;
	goto complete;

error:
	buffers_count = ARRAY_SIZE(encoder->output_buffers);

	for (i = 0; i < buffers_count; i++)
		v4l2_encoder_buffer_cleanup(&encoder->output_buffers[i]);

	v4l2_buffers_destroy(encoder->video_fd, encoder->output_type,
			     encoder->memory);

	buffers_count = ARRAY_SIZE(encoder->capture_buffers);

	for (i = 0; i < buffers_count; i++)
		v4l2_encoder_buffer_cleanup(&encoder->capture_buffers[i]);

	v4l2_buffers_destroy(encoder->video_fd, encoder->capture_type,
			     encoder->memory);

complete:
	return ret;
}

int v4l2_encoder_cleanup(struct v4l2_encoder *encoder)
{
	unsigned int buffers_count;
	unsigned int i;

	if (!encoder || !encoder->up)
		return -EINVAL;

	/* Cleanup output buffers. */

	buffers_count = ARRAY_SIZE(encoder->output_buffers);

	for (i = 0; i < buffers_count; i++)
		v4l2_encoder_buffer_cleanup(&encoder->output_buffers[i]);

	v4l2_buffers_destroy(encoder->video_fd, encoder->output_type,
			     encoder->memory);

	/* Cleanup capture buffers. */

	buffers_count = ARRAY_SIZE(encoder->capture_buffers);

	for (i = 0; i < buffers_count; i++)
		v4l2_encoder_buffer_cleanup(&encoder->capture_buffers[i]);

	v4l2_buffers_destroy(encoder->video_fd, encoder->capture_type,
			     encoder->memory);

	encoder->up = false;

	return 0;
}

int v4l2_encoder_probe(struct v4l2_encoder *encoder)
{
	bool check;
	int ret;

	if (!encoder || encoder->video_fd < 0)
		return -EINVAL;

	/* Probe capabilities. */

	ret = v4l2_capabilities_probe(encoder->video_fd, &encoder->capabilities,
				      encoder->driver, encoder->card);
	if (ret) {
		fprintf(stderr, "Failed to probe V4L2 capabilities\n");
		return ret;
	}

	printf("Probed driver %s card %s\n", encoder->driver, encoder->card);

	/* Check M2M support. */

	check = v4l2_capabilities_check(encoder->capabilities,
					V4L2_CAP_VIDEO_M2M);
	if (!check) {
		fprintf(stderr, "Missing V4L2 M2M support\n");
		return -1;
	}

	/* Check multi-plane support. */

	check = v4l2_capabilities_check(encoder->capabilities,
					V4L2_CAP_VIDEO_M2M_MPLANE);
	if (check) {
		encoder->output_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		encoder->capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	} else {
		encoder->output_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		encoder->capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	}

	/* Check memory type support. */

	encoder->memory = V4L2_MEMORY_MMAP;

	ret = v4l2_buffers_capabilities_probe(encoder->video_fd,
					      encoder->output_type,
					      encoder->memory,
					      &encoder->output_capabilities);
	if (ret)
		return ret;

	/* Check requests support. */

	check = v4l2_capabilities_check(encoder->output_capabilities,
					V4L2_BUF_CAP_SUPPORTS_REQUESTS);
	if (!check) {
		fprintf(stderr, "Missing output requests support\n");
		return -EINVAL;
	}

	/* Probe buffer capabilities. */

	ret = v4l2_buffers_capabilities_probe(encoder->video_fd,
					      encoder->capture_type,
					      encoder->memory,
					      &encoder->capture_capabilities);
	if (ret)
		return ret;

	/* Check coded pixel format. */

	check = v4l2_pixel_format_check(encoder->video_fd,
					encoder->capture_type,
					V4L2_PIX_FMT_H264);
	if (!check) {
		fprintf(stderr, "Missing H.264 pixel format\n");
		return -EINVAL;
	}

	printf("Selected driver %s card %s\n", encoder->driver, encoder->card);

	return 0;
}

static int media_device_probe(struct v4l2_encoder *encoder, struct udev *udev,
			      struct udev_device *device, int function)
{
	const char *path = udev_device_get_devnode(device);
	struct media_device_info device_info = { 0 };
	struct media_v2_topology topology = { 0 };
	struct media_v2_interface *interfaces = NULL;
	struct media_v2_entity *entities = NULL;
	struct media_v2_pad *pads = NULL;
	struct media_v2_link *links = NULL;
	struct media_v2_entity *encoder_entity;
	struct media_v2_interface *encoder_interface;
	struct media_v2_pad *sink_pad;
	struct media_v2_link *sink_link;
	struct media_v2_pad *source_pad;
	struct media_v2_link *source_link;
	const char *driver = "cedrus";
	int media_fd = -1;
	int video_fd = -1;
	dev_t devnum;
	int ret;

	media_fd = open(path, O_RDWR);
	if (media_fd < 0)
		return -errno;

	ret = media_device_info(media_fd, &device_info);
	if (ret)
		goto error;

	ret = strncmp(device_info.driver, driver, strlen(driver));
	if (ret)
		goto error;

	ret = media_topology_get(media_fd, &topology);
	if (ret)
		goto error;

	if (!topology.num_interfaces || !topology.num_entities ||
	    !topology.num_pads || !topology.num_links) {
		ret = -ENODEV;
		goto error;
	}

	interfaces = calloc(1, topology.num_interfaces * sizeof(*interfaces));
	if (!interfaces) {
		ret = -ENOMEM;
		goto error;
	}

	topology.ptr_interfaces = (__u64)interfaces;

	entities = calloc(1, topology.num_entities * sizeof(*entities));
	if (!entities) {
		ret = -ENOMEM;
		goto error;
	}

	topology.ptr_entities = (__u64)entities;

	pads = calloc(1, topology.num_pads * sizeof(*pads));
	if (!pads) {
		ret = -ENOMEM;
		goto error;
	}

	topology.ptr_pads = (__u64)pads;

	links = calloc(1, topology.num_links * sizeof(*links));
	if (!links) {
		ret = -ENOMEM;
		goto error;
	}

	topology.ptr_links = (__u64)links;

	ret = media_topology_get(media_fd, &topology);
	if (ret)
		goto error;

	encoder_entity = media_topology_entity_find_by_function(&topology,
								function);
	if (!encoder_entity) {
		ret = -ENODEV;
		goto error;
	}

	sink_pad = media_topology_pad_find_by_entity(&topology,
						     encoder_entity->id,
						     MEDIA_PAD_FL_SINK);
	if (!sink_pad) {
		ret = -ENODEV;
		goto error;
	}

	sink_link = media_topology_link_find_by_pad(&topology, sink_pad->id,
						    sink_pad->flags);
	if (!sink_link) {
		ret = -ENODEV;
		goto error;
	}

	source_pad = media_topology_pad_find_by_id(&topology,
						   sink_link->source_id);
	if (!source_pad) {
		ret = -ENODEV;
		goto error;
	}

	source_link = media_topology_link_find_by_entity(&topology,
							 source_pad->entity_id,
							 MEDIA_PAD_FL_SINK);
	if (!source_link) {
		ret = -ENODEV;
		goto error;
	}

	encoder_interface = media_topology_interface_find_by_id(&topology,
								source_link->source_id);
	if (!encoder_interface) {
		ret = -ENODEV;
		goto error;
	}

	devnum = makedev(encoder_interface->devnode.major,
			 encoder_interface->devnode.minor);

	device = udev_device_new_from_devnum(udev, 'c', devnum);
	if (!device) {
		ret = -ENODEV;
		goto error;
	}

	path = udev_device_get_devnode(device);

	video_fd = open(path, O_RDWR | O_NONBLOCK);
	if (video_fd < 0) {
		ret = -errno;
		goto error;
	}

	encoder->media_fd = media_fd;
	encoder->video_fd = video_fd;

	ret = 0;
	goto complete;

error:
	if (media_fd >= 0)
		close(media_fd);

	if (video_fd >= 0)
		close(video_fd);

complete:
	if (links)
		free(links);

	if (pads)
		free(pads);

	if (entities)
		free(entities);

	if (interfaces)
		free(interfaces);

	return ret;
}

int v4l2_encoder_open(struct v4l2_encoder *encoder)
{
	struct udev *udev = NULL;
	struct udev_enumerate *enumerate = NULL;
	struct udev_list_entry *devices;
	struct udev_list_entry *entry;
	int ret;

	if (!encoder)
		return -EINVAL;

	encoder->media_fd = -1;
	encoder->video_fd = -1;

	udev = udev_new();
	if (!udev)
		goto error;

	enumerate = udev_enumerate_new(udev);
	if (!enumerate)
		goto error;

	udev_enumerate_add_match_subsystem(enumerate, "media");
	udev_enumerate_scan_devices(enumerate);

	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(entry, devices) {
		struct udev_device *device;
		const char *path;

		path = udev_list_entry_get_name(entry);
		if (!path)
			continue;

		device = udev_device_new_from_syspath(udev, path);
		if (!device)
			continue;

		ret = media_device_probe(encoder, udev, device,
					 MEDIA_ENT_F_PROC_VIDEO_ENCODER);

		udev_device_unref(device);

		if (!ret)
			break;
	}

	if (encoder->media_fd < 0) {
		fprintf(stderr, "Failed to open encoder media device\n");
		goto error;
	}

	if (encoder->video_fd < 0) {
		fprintf(stderr, "Failed to open encoder video device\n");
		goto error;
	}

	encoder->bitstream_fd = open("bitstream.bin",
				     O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (encoder->bitstream_fd < 0) {
		fprintf(stderr, "Failed to open bitstream file\n");
		ret = -errno;
		goto error;
	}

	ret = 0;
	goto complete;

error:
	if (encoder->media_fd) {
		close(encoder->media_fd);
		encoder->media_fd = -1;
	}

	if (encoder->video_fd) {
		close(encoder->video_fd);
		encoder->video_fd = -1;
	}

	ret = -1;

complete:
	if (enumerate)
		udev_enumerate_unref(enumerate);

	if (udev)
		udev_unref(udev);

	return ret;
}

void v4l2_encoder_close(struct v4l2_encoder *encoder)
{
	if (!encoder)
		return;

	if (encoder->bitstream_fd > 0) {
		close(encoder->bitstream_fd);
		encoder->bitstream_fd = -1;
	}

	if (encoder->media_fd > 0) {
		close(encoder->media_fd);
		encoder->media_fd = -1;
	}

	if (encoder->video_fd > 0) {
		close(encoder->video_fd);
		encoder->video_fd = -1;
	}
}
