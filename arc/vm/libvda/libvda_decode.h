// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_LIBVDA_LIBVDA_DECODE_H_
#define ARC_VM_LIBVDA_LIBVDA_DECODE_H_

#include <stddef.h>
#include <stdint.h>

#include "arc/vm/libvda/libvda_export.h"

#ifdef __cplusplus
extern "C" {
#endif

// VDA implementation types.
typedef enum vda_impl_type {
  // A fake implementation for testing.
  FAKE,
  // A GpuArcVideoDecodeAccelerator-backed implementation.
  GAVDA
} vda_impl_type_t;

// Copy of VideoDecodeAccelerator::Result.
typedef enum vda_result {
  SUCCESS,
  ILLEGAL_STATE,
  INVALID_ARGUMENT,
  UNREADABLE_INPUT,
  PLATFORM_FAILURE,
  INSUFFICIENT_RESOURCES,
  CANCELLED
} vda_result_t;

// Copy of VideoCodecProfile.
typedef enum vda_profile {
  VIDEO_CODEC_PROFILE_UNKNOWN = -1,
  VIDEO_CODEC_PROFILE_MIN = VIDEO_CODEC_PROFILE_UNKNOWN,
  H264PROFILE_MIN = 0,
  H264PROFILE_BASELINE = H264PROFILE_MIN,
  H264PROFILE_MAIN = 1,
  H264PROFILE_EXTENDED = 2,
  H264PROFILE_HIGH = 3,
  H264PROFILE_HIGH10PROFILE = 4,
  H264PROFILE_HIGH422PROFILE = 5,
  H264PROFILE_HIGH444PREDICTIVEPROFILE = 6,
  H264PROFILE_SCALABLEBASELINE = 7,
  H264PROFILE_SCALABLEHIGH = 8,
  H264PROFILE_STEREOHIGH = 9,
  H264PROFILE_MULTIVIEWHIGH = 10,
  H264PROFILE_MAX = H264PROFILE_MULTIVIEWHIGH,
  VP8PROFILE_MIN = 11,
  VP8PROFILE_ANY = VP8PROFILE_MIN,
  VP8PROFILE_MAX = VP8PROFILE_ANY,
  VP9PROFILE_MIN = 12,
  VP9PROFILE_PROFILE0 = VP9PROFILE_MIN,
  VP9PROFILE_PROFILE1 = 13,
  VP9PROFILE_PROFILE2 = 14,
  VP9PROFILE_PROFILE3 = 15,
  VP9PROFILE_MAX = VP9PROFILE_PROFILE3,
  HEVCPROFILE_MIN = 16,
  HEVCPROFILE_MAIN = HEVCPROFILE_MIN,
  HEVCPROFILE_MAIN10 = 17,
  HEVCPROFILE_MAIN_STILL_PICTURE = 18,
  HEVCPROFILE_MAX = HEVCPROFILE_MAIN_STILL_PICTURE,
  DOLBYVISION_MIN = 19,
  DOLBYVISION_PROFILE0 = DOLBYVISION_MIN,
  DOLBYVISION_PROFILE4 = 20,
  DOLBYVISION_PROFILE5 = 21,
  DOLBYVISION_PROFILE7 = 22,
  DOLBYVISION_MAX = DOLBYVISION_PROFILE7,
  THEORAPROFILE_MIN = 23,
  THEORAPROFILE_ANY = THEORAPROFILE_MIN,
  THEORAPROFILE_MAX = THEORAPROFILE_ANY,
  AV1PROFILE_MIN = 24,
  AV1PROFILE_PROFILE_MAIN = AV1PROFILE_MIN,
  AV1PROFILE_PROFILE_HIGH = 25,
  AV1PROFILE_PROFILE_PRO = 26,
  AV1PROFILE_MAX = AV1PROFILE_PROFILE_PRO,
  VIDEO_CODEC_PROFILE_MAX = AV1PROFILE_PROFILE_PRO,
} vda_profile_t;

// Supported raw pixel formats.
typedef enum vda_pixel_format {
  YV12,
  NV12,
  PIXEL_FORMAT_MAX = NV12
} vda_pixel_format_t;

// Copy of VideoFramePlane.
typedef struct video_frame_plane {
  int32_t offset;
  int32_t stride;
} video_frame_plane_t;

// Possible VDA event types.
typedef enum vda_event_type {
  UNKNOWN,
  PROVIDE_PICTURE_BUFFERS,
  PICTURE_READY,
  NOTIFY_END_OF_BITSTREAM_BUFFER,
  NOTIFY_ERROR,
  RESET_RESPONSE,
  FLUSH_RESPONSE
} vda_event_type_t;

// Event data for event type PROVIDE_PICTURE_BUFFERS.
// Requests the users to provide output buffers.
typedef struct provide_picture_buffers_event_data {
  uint32_t min_num_buffers;
  int32_t width;
  int32_t height;

  // Visible rect coordinates.
  int32_t visible_rect_left;
  int32_t visible_rect_top;
  int32_t visible_rect_right;
  int32_t visible_rect_bottom;
} provide_picture_buffers_event_data_t;

// Event data for event type PICTURE_READY.
// Notifies the user of a decoded frame ready for display. These events will
// arrive in display order.
typedef struct picture_ready_event_data {
  int32_t picture_buffer_id;
  int32_t bitstream_id;
  int32_t crop_left;
  int32_t crop_top;
  int32_t crop_right;
  int32_t crop_bottom;
} picture_ready_event_data_t;

// Union of possible events.
typedef union vda_event_data {
  // Event data for event type PROVIDE_PICTURE_BUFFERS.
  provide_picture_buffers_event_data_t provide_picture_buffers;
  // Event data for event type PICTURE_READY.
  picture_ready_event_data_t picture_ready;
  // Event data for event type NOTIFY_END_OF_BITSTREAM_BUFFER
  int32_t bitstream_id;
  // Event data for event types NOTIFY_ERROR, RESET_RESPONSE, or FLUSH_RESPONSE.
  vda_result_t result;
} vda_event_data_t;

// VDA input format with profile and min/max resolution.
typedef struct vda_input_format {
  vda_profile_t profile;
  uint32_t min_width;
  uint32_t min_height;
  uint32_t max_width;
  uint32_t max_height;
} vda_input_format_t;

// A struct representing a single VDA event.
typedef struct vda_event {
  vda_event_type_t event_type;
  vda_event_data_t event_data;
} vda_event_t;

// Media capabilities of a VDA implementation.
typedef struct vda_capabilities {
  // Supported input formats.
  size_t num_input_formats;
  const vda_input_format_t* input_formats;

  // Supported output formats, valid for any supported input format.
  size_t num_output_formats;
  const vda_pixel_format_t* output_formats;
} vda_capabilities_t;

// VDA decode session info returned by init_decode_session().
typedef struct vda_session_info {
  // A decode session context used for decoding.
  void* ctx;
  // Event pipe file descriptor. When new decode session events occur,
  // vda_event_t objects can be read from the fd.
  int event_pipe_fd;
} vda_session_info_t;

/*
 * Global implementation object methods
 */

// Initializes libvda and returns an implementation object of type |impl_type|.
// The returned implementation object can be used as a global context
// for creating new decode sessions and querying underlying implementation
// capabilities. If the requested implementation type is not available,
// NULL is returned. Note that for the impl_type GAVDA, it is expected that
// only one implementation object exists at a time.
// This function and deinitialize() should be called from the same thread.
void* LIBVDA_EXPORT initialize(vda_impl_type_t impl_type);

// Deinitializes the implementation object. The provided object will be
// destroyed and no other calls will be possible. This function and initialize()
// should be called from the same thread.
void LIBVDA_EXPORT deinitialize(void* impl);

// Returns the underlying implementation capabilities of the provided
// implementation object. Ownership of the returned vda_capabilities_t object
// is retained by the library. When deinitialize() is called on |impl|, the
// capabilities object is deleted.
const vda_capabilities_t* LIBVDA_EXPORT get_vda_capabilities(void* impl);

// Creates and initializes a new decode session that supports decoding profile
// |profile|, using the provided implementation object. The returned
// vda_session_info_t object contains a decode session context
// which can be passed to vda_decode, vda_use_output_buffer, vda_flush,
// and vda_reset to perform decoding.  NULL is returned if an error occurs
// and a decode session could not be initialized.
vda_session_info_t* LIBVDA_EXPORT init_decode_session(void* impl,
                                                      vda_profile_t profile);

// Closes a previously created decode session specified by |session_info|.
void LIBVDA_EXPORT close_decode_session(void* impl,
                                        vda_session_info_t* session_info);

/*
 * Asychronous decoder session context functions
 */

// Decodes the frame pointed to by |fd| for decode session context |ctx|.
// |offset| and |bytes_used| should point to the buffer offset and the size of
// the frame. Ownership of |fd| is passed to the library. |fd| will be closed
// after decoding has occurred and the fd is no longer needed.
// Returns SUCCESS when the decode request has been processed, else
// the error is indicated.
vda_result_t LIBVDA_EXPORT vda_decode(void* ctx,
                                      int32_t bitstream_id,
                                      int fd,
                                      uint32_t offset,
                                      uint32_t bytes_used);

// Sets the number of expected output buffers to |num_output_buffers|. This call
// should be followed by |num_output_buffers| invocations of
// vda_use_output_buffer.
vda_result_t LIBVDA_EXPORT
vda_set_output_buffer_count(void* ctx, size_t num_output_buffers);

// Provides an output buffer |fd| for decoded frames in decode session context
// |ctx| where |format| is a valid output pixel format listed in
// get_vda_capabilities, and |planes| is a pointer to an array of |num_planes|
// objects. |planes| ownership is retained by the caller.
vda_result_t LIBVDA_EXPORT vda_use_output_buffer(void* ctx,
                                                 int32_t picture_buffer_id,
                                                 vda_pixel_format_t format,
                                                 int fd,
                                                 size_t num_planes,
                                                 video_frame_plane_t* planes);

// Returns output buffer with id |picture_buffer_id| for reuse.
vda_result_t LIBVDA_EXPORT vda_reuse_output_buffer(void* ctx,
                                                   int32_t picture_buffer_id);

// Flushes the decode session context |ctx|. When this operation has completed,
// an event of type FLUSH_RESPONSE is sent.
vda_result_t LIBVDA_EXPORT vda_flush(void* ctx);

// Resets the decode session context |ctx|. Pending buffers will not be decoded.
// When this operation has completed, an event of type RESET_RESPONSE is sent
// with the result.
// If vda_reset() is called before a vda_flush() is completed, the flush
// request will be cancelled ie. an event of type FLUSH_RESPONSE with result
// CANCELLED will be sent.
vda_result_t LIBVDA_EXPORT vda_reset(void* ctx);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ARC_VM_LIBVDA_LIBVDA_DECODE_H_
