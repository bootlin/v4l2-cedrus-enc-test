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

#ifndef _V4L2_H_
#define _V4L2_H_

#include <stdbool.h>
#include <stdint.h>

#include <linux/videodev2.h>

/* Type */

bool v4l2_type_mplane_check(unsigned int type);

/* Capabilities */

int v4l2_capabilities_probe(int video_fd, unsigned int *capabilities,
			    char *driver, char *card);
bool v4l2_capabilities_check(unsigned int capabilities_probed,
			     unsigned int capabilities);

/* Pixel Format */

int v4l2_pixel_format_enum(int video_fd, unsigned int type, unsigned int index,
			   unsigned int *pixel_format, char *description);
bool v4l2_pixel_format_check(int video_fd, unsigned int type,
			     unsigned int pixel_format);

/* Format */

int v4l2_format_try(int video_fd, struct v4l2_format *format);
int v4l2_format_set(int video_fd, struct v4l2_format *format);
int v4l2_format_get(int video_fd, struct v4l2_format *format);
void v4l2_format_setup_base(struct v4l2_format *format, unsigned int type);
void v4l2_format_setup_pixel(struct v4l2_format *format, unsigned int width,
			     unsigned int height, unsigned int pixel_format);
int v4l2_format_setup_sizeimage(struct v4l2_format *format,
				unsigned int plane_index,
				unsigned int sizeimage);
int v4l2_format_pixel(struct v4l2_format *format, unsigned int *width,
		      unsigned int *height, unsigned int *pixel_format);
int v4l2_format_pixel_format(struct v4l2_format *format);
int v4l2_format_planes_count(struct v4l2_format *format);

/* Selection */

int v4l2_selection_set(int video_fd, struct v4l2_selection *selection);
int v4l2_selection_get(int video_fd, struct v4l2_selection *selection);
void v4l2_selection_setup_base(struct v4l2_selection *selection,
			       unsigned int type, unsigned int target);
void v4l2_selection_setup_rect(struct v4l2_selection *selection,
			       unsigned int top, unsigned int left,
			       unsigned int width, unsigned int height);

/* Control */

int v4l2_control_set(int video_fd, struct v4l2_control *control);
int v4l2_control_get(int video_fd, struct v4l2_control *control);
void v4l2_control_setup_base(struct v4l2_control *control, unsigned int id);
void v4l2_control_setup_value(struct v4l2_control *control, int value);
int v4l2_control_value(struct v4l2_control *control);

/* Extended Controls */

int v4l2_ext_controls_set(int video_fd, struct v4l2_ext_controls *ext_controls);
int v4l2_ext_controls_get(int video_fd, struct v4l2_ext_controls *ext_controls);
int v4l2_ext_controls_try(int video_fd, struct v4l2_ext_controls *ext_controls);
void v4l2_ext_control_setup_compound(struct v4l2_ext_control *control,
				     void *data, unsigned int size);
void v4l2_ext_control_setup_base(struct v4l2_ext_control *control,
				 unsigned int id);
void v4l2_ext_controls_setup(struct v4l2_ext_controls *ext_controls,
			     struct v4l2_ext_control *controls,
			     unsigned int controls_count);
void v4l2_ext_controls_request_attach(struct v4l2_ext_controls *ext_controls,
				      int request_fd);
void v4l2_ext_controls_request_detach(struct v4l2_ext_controls *ext_controls);

/* Parm */

int v4l2_parm_set(int video_fd, struct v4l2_streamparm *streamparm);
int v4l2_parm_get(int video_fd, struct v4l2_streamparm *streamparm);
void v4l2_parm_setup_base(struct v4l2_streamparm *streamparm,
			  unsigned int type);

/* Buffers */

int v4l2_buffers_create(int video_fd, unsigned int type, unsigned int memory,
			struct v4l2_format *format, unsigned int count,
			unsigned int *index);
int v4l2_buffers_request(int video_fd, unsigned int type, unsigned int memory,
			 unsigned int count);
int v4l2_buffers_destroy(int video_fd, unsigned int type, unsigned int memory);
int v4l2_buffers_capabilities_probe(int video_fd, unsigned int type,
				    unsigned int memory,
				    unsigned int *capabilities);

/* Buffer */

int v4l2_buffer_query(int video_fd, struct v4l2_buffer *buffer);
int v4l2_buffer_queue(int video_fd, struct v4l2_buffer *buffer);
int v4l2_buffer_dequeue(int video_fd, struct v4l2_buffer *buffer);
void v4l2_buffer_setup_base(struct v4l2_buffer *buffer, unsigned int type,
			    unsigned int memory);
void v4l2_buffer_setup_index(struct v4l2_buffer *buffer, unsigned int index);
void v4l2_buffer_setup_planes(struct v4l2_buffer *buffer, unsigned int type,
			      struct v4l2_plane *planes,
			      unsigned int planes_count);
int v4l2_buffer_setup_plane_length_used(struct v4l2_buffer *buffer,
					unsigned int plane_index,
					unsigned int length);
void v4l2_buffer_setup_userptr(struct v4l2_buffer *buffer, void *pointer,
			       unsigned int length);
void v4l2_buffer_setup_timestamp(struct v4l2_buffer *buffer, uint64_t timestamp);
void v4l2_buffer_request_attach(struct v4l2_buffer *buffer, int request_fd);
void v4l2_buffer_request_detach(struct v4l2_buffer *buffer);
bool v4l2_buffer_error_check(struct v4l2_buffer *buffer);
int v4l2_buffer_plane_offset(struct v4l2_buffer *buffer,
			     unsigned int plane_index, unsigned int *offset);
int v4l2_buffer_plane_length(struct v4l2_buffer *buffer,
			     unsigned int plane_index, unsigned int *length);
int v4l2_buffer_plane_length_used(struct v4l2_buffer *buffer,
				  unsigned int plane_index,
				  unsigned int *length);
void v4l2_buffer_timestamp(struct v4l2_buffer *buffer, uint64_t *timestamp);

/* Stream */

int v4l2_stream_on(int video_fd, unsigned int type);
int v4l2_stream_off(int video_fd, unsigned int type);

/* Poll */

int v4l2_poll(int video_fd, struct timeval *timeout);

#endif
