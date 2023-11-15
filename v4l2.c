/*
 * Copyright (C) 2020 Paul Kocialkowski <contact@paulk.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <linux/videodev2.h>
#include <linux/media.h>

#include <v4l2.h>

/* Type */

bool v4l2_type_mplane_check(unsigned int type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		return true;

	default:
		return false;
	}
}

unsigned int v4l2_type_base(unsigned int type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return V4L2_BUF_TYPE_VIDEO_OUTPUT;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		return V4L2_BUF_TYPE_VIDEO_CAPTURE;
	default:
		return type;
	}
}

/* Capabilities */

int v4l2_capabilities_probe(int video_fd, unsigned int *capabilities,
			    char *driver, char *card)
{
	struct v4l2_capability capability = { 0 };
	int ret;

	if (!capabilities)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_QUERYCAP, &capability);
	if (ret < 0)
		return -errno;

	if (capability.capabilities & V4L2_CAP_DEVICE_CAPS)
		*capabilities = capability.device_caps;
	else
		*capabilities = capability.capabilities;

	if (driver)
		strncpy(driver, capability.driver, sizeof(capability.driver));

	if (card)
		strncpy(card, capability.card, sizeof(capability.card));

	return 0;
}


bool v4l2_capabilities_check(unsigned int capabilities_probed,
			     unsigned int capabilities)
{
	unsigned int capabilities_mask = capabilities_probed & capabilities;

	return capabilities_mask == capabilities;
}

/* Pixel Format */

int v4l2_pixel_format_enum(int video_fd, unsigned int type, unsigned int index,
			   unsigned int *pixel_format, char *description)
{
	struct v4l2_fmtdesc fmtdesc = { 0 };
	int ret;

	if (!pixel_format)
		return -EINVAL;

	fmtdesc.type = type;
	fmtdesc.index = index;

	ret = ioctl(video_fd, VIDIOC_ENUM_FMT, &fmtdesc);
	if (ret)
		return -errno;

	*pixel_format = fmtdesc.pixelformat;

	if (description)
		strncpy(description, fmtdesc.description,
			sizeof(fmtdesc.description));

	return 0;
}

bool v4l2_pixel_format_check(int video_fd, unsigned int type,
			     unsigned int pixel_format)
{
	unsigned int index = 0;
	unsigned int ret;

	do {
		unsigned int pixel_format_enum;

		ret = v4l2_pixel_format_enum(video_fd, type, index,
					     &pixel_format_enum, NULL);
		if (ret)
			break;

		if (pixel_format_enum == pixel_format)
			return true;

		index++;
	} while (ret >= 0);

	return false;
}

/* Format */

int v4l2_format_try(int video_fd, struct v4l2_format *format)
{
	int ret;

	if (!format)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_TRY_FMT, format);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_format_set(int video_fd, struct v4l2_format *format)
{
	int ret;

	if (!format)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_S_FMT, format);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_format_get(int video_fd, struct v4l2_format *format)
{
	int ret;

	if (!format)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_G_FMT, format);
	if (ret)
		return -errno;

	return 0;
}

void v4l2_format_setup_base(struct v4l2_format *format, unsigned int type)
{
	if (!format)
		return;

	memset(format, 0, sizeof(*format));

	format->type = type;
}

void v4l2_format_setup_pixel(struct v4l2_format *format, unsigned int width,
			     unsigned int height, unsigned int pixel_format)
{
	bool mplane_check;

	if (!format)
		return;

	mplane_check = v4l2_type_mplane_check(format->type);
	if (mplane_check) {
		format->fmt.pix_mp.width = width;
		format->fmt.pix_mp.height = height;
		format->fmt.pix_mp.pixelformat = pixel_format;
	} else {
		format->fmt.pix.width = width;
		format->fmt.pix.height = height;
		format->fmt.pix.pixelformat = pixel_format;
	}
}

int v4l2_format_setup_sizeimage(struct v4l2_format *format,
				unsigned int plane_index,
				unsigned int sizeimage)
{
	bool mplane_check;

	if (!format)
		return -EINVAL;

	mplane_check = v4l2_type_mplane_check(format->type);
	if (mplane_check) {
		if (plane_index >= format->fmt.pix_mp.num_planes)
			return -EINVAL;

		format->fmt.pix_mp.plane_fmt[plane_index].sizeimage = sizeimage;
	} else {
		if (plane_index > 0)
			return -EINVAL;

		format->fmt.pix.sizeimage = sizeimage;
	}

	return 0;
}

int v4l2_format_pixel(struct v4l2_format *format, unsigned int *width,
		      unsigned int *height, unsigned int *pixel_format)
{
	bool mplane_check;

	if (!format)
		return -EINVAL;

	mplane_check = v4l2_type_mplane_check(format->type);

	if (width) {
		if (mplane_check)
			*width = format->fmt.pix_mp.width;
		else
			*width = format->fmt.pix.width;
	}

	if (height) {
		if (mplane_check)
			*height = format->fmt.pix_mp.height;
		else
			*height = format->fmt.pix.height;
	}

	if (pixel_format) {
		if (mplane_check)
			*pixel_format = format->fmt.pix_mp.pixelformat;
		else
			*pixel_format = format->fmt.pix.pixelformat;
	}

	return 0;
}

int v4l2_format_pixel_format(struct v4l2_format *format)
{
	bool mplane_check;

	if (!format)
		return -EINVAL;

	if (mplane_check)
		return format->fmt.pix_mp.pixelformat;
	else
		return format->fmt.pix.pixelformat;
}

int v4l2_format_planes_count(struct v4l2_format *format)
{
	bool mplane_check;

	if (!format)
		return -EINVAL;

	mplane_check = v4l2_type_mplane_check(format->type);
	if (mplane_check)
		return format->fmt.pix_mp.num_planes;
	else
		return 1;
}

/* Selection */

int v4l2_selection_set(int video_fd, struct v4l2_selection *selection)
{
	int ret;

	if (!selection)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_S_SELECTION, selection);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_selection_get(int video_fd, struct v4l2_selection *selection)
{
	int ret;

	if (!selection)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_G_SELECTION, selection);
	if (ret)
		return -errno;

	return 0;
}

void v4l2_selection_setup_base(struct v4l2_selection *selection,
			       unsigned int type, unsigned int target)
{
	if (!selection)
		return;

	memset(selection, 0, sizeof(*selection));

	selection->type = v4l2_type_base(type);
	selection->target = target;
}

void v4l2_selection_setup_rect(struct v4l2_selection *selection,
			       unsigned int top, unsigned int left,
			       unsigned int width, unsigned int height)
{
	if (!selection)
		return;

	selection->r.top = top;
	selection->r.left = left;
	selection->r.width = width;
	selection->r.height = height;
}

/* Control */

int v4l2_control_set(int video_fd, struct v4l2_control *control)
{
	int ret;

	if (!control)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_S_CTRL, control);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_control_get(int video_fd, struct v4l2_control *control)
{
	int ret;

	if (!control)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_G_CTRL, control);
	if (ret)
		return -errno;

	return 0;
}

void v4l2_control_setup_base(struct v4l2_control *control, unsigned int id)
{
	if (!control)
		return;

	memset(control, 0, sizeof(*control));

	control->id = id;
}

void v4l2_control_setup_value(struct v4l2_control *control, int value)
{
	if (!control)
		return;

	control->value = value;
}

int v4l2_control_value(struct v4l2_control *control)
{
	if (!control)
		return -EINVAL;

	return control->value;
}

/* Extended Controls */

int v4l2_ext_controls_set(int video_fd, struct v4l2_ext_controls *ext_controls)
{
	int ret;

	if (!ext_controls)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_S_EXT_CTRLS, ext_controls);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_ext_controls_get(int video_fd, struct v4l2_ext_controls *ext_controls)
{
	int ret;

	if (!ext_controls)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_G_EXT_CTRLS, ext_controls);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_ext_controls_try(int video_fd, struct v4l2_ext_controls *ext_controls)
{
	int ret;

	if (!ext_controls)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_TRY_EXT_CTRLS, ext_controls);
	if (ret)
		return -errno;

	return 0;
}

void v4l2_ext_controls_setup(struct v4l2_ext_controls *ext_controls,
			     struct v4l2_ext_control *controls,
			     unsigned int controls_count)
{
	if (!controls)
		return;

	ext_controls->controls = controls;
	ext_controls->count = controls_count;
}

void v4l2_ext_control_setup_compound(struct v4l2_ext_control *control,
				     void *data, unsigned int size)
{
	if (!control)
		return;

	control->ptr = data;
	control->size = size;
}

void v4l2_ext_control_setup_base(struct v4l2_ext_control *control,
				 unsigned int id)
{
	if (!control)
		return;

	memset(control, 0, sizeof(*control));

	control->id = id;
}

void v4l2_ext_controls_request_attach(struct v4l2_ext_controls *ext_controls,
				      int request_fd)
{
	if (!ext_controls)
		return;

	ext_controls->which = V4L2_CTRL_WHICH_REQUEST_VAL;
	ext_controls->request_fd = request_fd;
}

void v4l2_ext_controls_request_detach(struct v4l2_ext_controls *ext_controls)
{
	if (!ext_controls)
		return;

	if (ext_controls->which == V4L2_CTRL_WHICH_REQUEST_VAL)
		ext_controls->which = 0;

	ext_controls->request_fd = -1;
}

/* Parm */

void v4l2_parm_setup_base(struct v4l2_streamparm *streamparm, unsigned int type)
{
	if (!streamparm)
		return;

	memset(streamparm, 0, sizeof(*streamparm));

	streamparm->type = type;
}

int v4l2_parm_set(int video_fd, struct v4l2_streamparm *streamparm)
{
	int ret;

	if (!streamparm)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_S_PARM, streamparm);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_parm_get(int video_fd, struct v4l2_streamparm *streamparm)
{
	int ret;

	if (!streamparm)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_G_PARM, streamparm);
	if (ret)
		return -errno;

	return 0;
}

/* Buffers */

int v4l2_buffers_create(int video_fd, unsigned int type, unsigned int memory,
			struct v4l2_format *format, unsigned int count,
			unsigned int *index)
{
	struct v4l2_create_buffers create_buffers = { 0 };
	int ret;

	if (format) {
		create_buffers.format = *format;
	} else {
		ret = ioctl(video_fd, VIDIOC_G_FMT, &create_buffers.format);
		if (ret)
			return -errno;
	}

	create_buffers.format.type = type;
	create_buffers.memory = memory;
	create_buffers.count = count;

	ret = ioctl(video_fd, VIDIOC_CREATE_BUFS, &create_buffers);
	if (ret)
		return -errno;

	if (index)
		*index = create_buffers.index;

	return 0;
}

int v4l2_buffers_request(int video_fd, unsigned int type, unsigned int memory,
			 unsigned int count)
{
	struct v4l2_requestbuffers requestbuffers = { 0 };
	int ret;

	requestbuffers.type = type;
	requestbuffers.memory = memory;
	requestbuffers.count = count;

	ret = ioctl(video_fd, VIDIOC_REQBUFS, &requestbuffers);
	if (ret)
		return -errno;

	return 0;
}


int v4l2_buffers_destroy(int video_fd, unsigned int type, unsigned int memory)
{
	struct v4l2_requestbuffers requestbuffers = { 0 };
	int ret;

	requestbuffers.type = type;
	requestbuffers.memory = memory;
	requestbuffers.count = 0;

	ret = ioctl(video_fd, VIDIOC_REQBUFS, &requestbuffers);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_buffers_capabilities_probe(int video_fd, unsigned int type,
				    unsigned int memory,
				    unsigned int *capabilities)
{
	struct v4l2_create_buffers create_buffers = { 0 };
	int ret;

	if (!capabilities)
		return -EINVAL;

	create_buffers.format.type = type;
	create_buffers.memory = memory;
	create_buffers.count = 0;

	ret = ioctl(video_fd, VIDIOC_CREATE_BUFS, &create_buffers);
	if (ret)
		return -errno;

	if (create_buffers.capabilities)
		*capabilities = create_buffers.capabilities;
	else
		*capabilities = V4L2_BUF_CAP_SUPPORTS_MMAP;

	return 0;
}

/* Buffer */

int v4l2_buffer_query(int video_fd, struct v4l2_buffer *buffer)
{
	int ret;

	if (!buffer)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_QUERYBUF, buffer);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_buffer_queue(int video_fd, struct v4l2_buffer *buffer)
{
	int ret;

	if (!buffer)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_QBUF, buffer);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_buffer_dequeue(int video_fd, struct v4l2_buffer *buffer)
{
	int ret;

	if (!buffer)
		return -EINVAL;

	ret = ioctl(video_fd, VIDIOC_DQBUF, buffer);
	if (ret)
		return -errno;

	return 0;
}

void v4l2_buffer_setup_base(struct v4l2_buffer *buffer, unsigned int type,
			    unsigned int memory)
{

	if (!buffer)
		return;

	memset(buffer, 0, sizeof(*buffer));

	buffer->type = type;
	buffer->memory = memory;
}

void v4l2_buffer_setup_index(struct v4l2_buffer *buffer, unsigned int index)
{

	if (!buffer)
		return;

	buffer->index = index;
}

void v4l2_buffer_setup_planes(struct v4l2_buffer *buffer, unsigned int type,
			      struct v4l2_plane *planes,
			      unsigned int planes_count)
{
	bool mplane_check;

	if (!buffer)
		return;

	mplane_check = v4l2_type_mplane_check(type);
	if (mplane_check && planes) {
		buffer->m.planes = planes;
		buffer->length = planes_count;
	}
}

int v4l2_buffer_setup_plane_length_used(struct v4l2_buffer *buffer,
					unsigned int plane_index,
					unsigned int length)
{
	bool mplane_check;

	if (!buffer || !length)
		return -EINVAL;

	mplane_check = v4l2_type_mplane_check(buffer->type);
	if (mplane_check) {
		if (!buffer->m.planes || plane_index >= buffer->length)
			return -EINVAL;

		buffer->m.planes[plane_index].bytesused = length;
	} else {
		if (plane_index > 0)
			return -EINVAL;

		buffer->bytesused = length;
	}

	return 0;
}
void v4l2_buffer_setup_userptr(struct v4l2_buffer *buffer, void *pointer,
			       unsigned int length)
{
	if (!buffer)
		return;

	buffer->m.userptr = (long unsigned int)pointer;
	buffer->length = length;
}

void v4l2_buffer_setup_timestamp(struct v4l2_buffer *buffer, uint64_t timestamp)
{
	if (!buffer)
		return;

	buffer->timestamp.tv_sec = timestamp / 1000000000ULL;
	buffer->timestamp.tv_usec = (timestamp % 1000000000ULL) / 1000UL;
}

void v4l2_buffer_request_attach(struct v4l2_buffer *buffer, int request_fd)
{
	if (!buffer)
		return;

	buffer->flags |= V4L2_BUF_FLAG_REQUEST_FD;
	buffer->request_fd = request_fd;
}

void v4l2_buffer_request_detach(struct v4l2_buffer *buffer)
{
	if (!buffer)
		return;

	buffer->flags &= ~V4L2_BUF_FLAG_REQUEST_FD;
	buffer->request_fd = -1;
}

bool v4l2_buffer_error_check(struct v4l2_buffer *buffer)
{
	if (!buffer)
		return true;

	return buffer->flags & V4L2_BUF_FLAG_ERROR;
}

int v4l2_buffer_plane_offset(struct v4l2_buffer *buffer,
			     unsigned int plane_index, unsigned int *offset)
{
	bool mplane_check;

	if (!buffer || !offset)
		return -EINVAL;

	mplane_check = v4l2_type_mplane_check(buffer->type);
	if (mplane_check) {
		if (!buffer->m.planes || plane_index >= buffer->length)
			return -EINVAL;

		*offset = buffer->m.planes[plane_index].m.mem_offset;
	} else {
		if (plane_index > 0)
			return -EINVAL;

		*offset = buffer->m.offset;
	}

	return 0;
}

int v4l2_buffer_plane_length(struct v4l2_buffer *buffer,
			     unsigned int plane_index, unsigned int *length)
{
	bool mplane_check;

	if (!buffer || !length)
		return -EINVAL;

	mplane_check = v4l2_type_mplane_check(buffer->type);
	if (mplane_check) {
		if (!buffer->m.planes || plane_index >= buffer->length)
			return -EINVAL;

		*length = buffer->m.planes[plane_index].length;
	} else {
		if (plane_index > 0)
			return -EINVAL;

		*length = buffer->length;
	}

	return 0;
}

int v4l2_buffer_plane_length_used(struct v4l2_buffer *buffer,
				  unsigned int plane_index,
				  unsigned int *length)
{
	bool mplane_check;

	if (!buffer || !length)
		return -EINVAL;

	mplane_check = v4l2_type_mplane_check(buffer->type);
	if (mplane_check) {
		if (!buffer->m.planes || plane_index >= buffer->length)
			return -EINVAL;

		*length = buffer->m.planes[plane_index].bytesused;
	} else {
		if (plane_index > 0)
			return -EINVAL;

		*length = buffer->bytesused;
	}

	return 0;
}

void v4l2_buffer_timestamp(struct v4l2_buffer *buffer, uint64_t *timestamp)
{
	if (!buffer || !timestamp)
		return;

	*timestamp = v4l2_timeval_to_ns(&buffer->timestamp);
}

/* Stream */

int v4l2_stream_on(int video_fd, unsigned int type)
{
	int ret;

	ret = ioctl(video_fd, VIDIOC_STREAMON, &type);
	if (ret)
		return -errno;

	return 0;
}

int v4l2_stream_off(int video_fd, unsigned int type)
{
	int ret;

	ret = ioctl(video_fd, VIDIOC_STREAMOFF, &type);
	if (ret)
		return -errno;

	return 0;
}

/* Poll */

int v4l2_poll(int video_fd, struct timeval *timeout)
{
	fd_set read_fds;
	int ret;

	FD_ZERO(&read_fds);
	FD_SET(video_fd, &read_fds);

	ret = select(video_fd + 1, &read_fds, NULL, NULL, timeout);
	if (ret < 0)
		return -errno;

	if (!FD_ISSET(video_fd, &read_fds))
		return 0;

	return ret;
}
