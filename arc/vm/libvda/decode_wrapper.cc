// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/libvda/decode_wrapper.h"

#include <fcntl.h>

#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>

#include "arc/vm/libvda/decode/fake/fake_vda_impl.h"
#include "arc/vm/libvda/decode/gpu/gpu_vda_impl.h"

namespace arc {

VdaImpl::VdaImpl() {
  capabilities_.num_input_formats = 0;
  capabilities_.input_formats = nullptr;
  capabilities_.num_output_formats = 0;
  capabilities_.output_formats = nullptr;
}

VdaImpl::~VdaImpl() = default;

const vda_capabilities_t* const VdaImpl::GetCapabilities() {
  return &capabilities_;
}

VdaContext::VdaContext() : event_write_thread_("EventWriteThread") {
  int pipe_fds[2];
  CHECK_EQ(pipe2(pipe_fds, O_CLOEXEC), 0);

  event_read_fd_.reset(pipe_fds[0]);
  event_write_fd_.reset(pipe_fds[1]);

  // Start the dedicated event write thread for this decode session context.
  CHECK(event_write_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_DEFAULT, 0)));
}

VdaContext::~VdaContext() = default;

int VdaContext::GetEventFd() {
  return event_read_fd_.get();
}

// This method should only be called on the |event_write_thread| in order to
// write |event|. This is done so that all events are sent in sequence
// (important for PICTURE_READY events), and to ensure that a full write of
// |event| is always done as a single operation. Using the IPC thread was
// considered, but then in cases where the pipe buffer is close to being full,
// write() could block, which would not be acceptable to the IPC thread.
// Setting the pipe to non-blocking mode was also considered, but then
// we would have to re-post a new task to complete the write which could
// cause ordering issues.
void VdaContext::WriteOnEventWriteThread(vda_event_t event) {
  DCHECK(event_write_thread_.task_runner()->BelongsToCurrentThread());
  CHECK(base::WriteFileDescriptor(event_write_fd_.get(),
                                  reinterpret_cast<const char*>(&event),
                                  sizeof(vda_event_t)));
}

void VdaContext::DispatchProvidePictureBuffers(uint32_t min_num_buffers,
                                               int32_t width,
                                               int32_t height,
                                               int32_t visible_rect_left,
                                               int32_t visible_rect_top,
                                               int32_t visible_rect_right,
                                               int32_t visible_rect_bottom) {
  vda_event_t event;
  event.event_type = PROVIDE_PICTURE_BUFFERS;
  event.event_data.provide_picture_buffers.min_num_buffers = min_num_buffers;
  event.event_data.provide_picture_buffers.width = width;
  event.event_data.provide_picture_buffers.height = height;
  event.event_data.provide_picture_buffers.visible_rect_left =
      visible_rect_left;
  event.event_data.provide_picture_buffers.visible_rect_top = visible_rect_top;
  event.event_data.provide_picture_buffers.visible_rect_right =
      visible_rect_right;
  event.event_data.provide_picture_buffers.visible_rect_bottom =
      visible_rect_bottom;
  event_write_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VdaContext::WriteOnEventWriteThread,
                                base::Unretained(this), std::move(event)));
}

void VdaContext::DispatchPictureReady(int32_t picture_buffer_id,
                                      int32_t bitstream_id,
                                      int crop_left,
                                      int crop_top,
                                      int crop_right,
                                      int crop_bottom) {
  vda_event_t event;
  event.event_type = PICTURE_READY;
  event.event_data.picture_ready.picture_buffer_id = picture_buffer_id;
  event.event_data.picture_ready.bitstream_id = bitstream_id;
  event.event_data.picture_ready.crop_left = crop_left;
  event.event_data.picture_ready.crop_top = crop_top;
  event.event_data.picture_ready.crop_right = crop_right;
  event.event_data.picture_ready.crop_bottom = crop_bottom;
  event_write_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VdaContext::WriteOnEventWriteThread,
                                base::Unretained(this), std::move(event)));
}

void VdaContext::DispatchNotifyEndOfBitstreamBuffer(int32_t bitstream_id) {
  vda_event_t event;
  event.event_type = NOTIFY_END_OF_BITSTREAM_BUFFER;
  event.event_data.bitstream_id = bitstream_id;
  event_write_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VdaContext::WriteOnEventWriteThread,
                                base::Unretained(this), std::move(event)));
}

void VdaContext::DispatchNotifyError(vda_result_t result) {
  vda_event_t event;
  event.event_type = NOTIFY_ERROR;
  event.event_data.result = result;
  event_write_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VdaContext::WriteOnEventWriteThread,
                                base::Unretained(this), std::move(event)));
}

void VdaContext::DispatchResetResponse(vda_result_t result) {
  vda_event_t event;
  event.event_type = RESET_RESPONSE;
  event.event_data.result = result;
  event_write_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VdaContext::WriteOnEventWriteThread,
                                base::Unretained(this), std::move(event)));
}

void VdaContext::DispatchFlushResponse(vda_result_t result) {
  vda_event_t event;
  event.event_type = FLUSH_RESPONSE;
  event.event_data.result = result;
  event_write_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VdaContext::WriteOnEventWriteThread,
                                base::Unretained(this), std::move(event)));
}

}  // namespace arc

void* initialize(vda_impl_type_t impl_type) {
  switch (impl_type) {
    case FAKE:
      return arc::FakeVdaImpl::Create();
    case GAVDA:
      return arc::GpuVdaImpl::Create();
    default:
      LOG(ERROR) << "Unknown impl type " << impl_type;
      return nullptr;
  }
}

void deinitialize(void* impl) {
  arc::VdaImpl* cast_impl = static_cast<arc::VdaImpl*>(impl);
  delete cast_impl;
}

const vda_capabilities_t* get_vda_capabilities(void* impl) {
  return static_cast<arc::VdaImpl*>(impl)->GetCapabilities();
}

vda_session_info_t* init_decode_session(void* impl, vda_profile profile) {
  arc::VdaContext* context =
      static_cast<arc::VdaImpl*>(impl)->InitDecodeSession(profile);
  if (!context)
    return nullptr;
  vda_session_info_t* session_info = new vda_session_info_t();
  session_info->ctx = context;
  session_info->event_pipe_fd = context->GetEventFd();
  return session_info;
}

void close_decode_session(void* impl, vda_session_info_t* session_info) {
  static_cast<arc::VdaImpl*>(impl)->CloseDecodeSession(
      static_cast<arc::VdaContext*>(session_info->ctx));
  delete session_info;
}

vda_result_t vda_decode(void* ctx,
                        int32_t bitstream_id,
                        int fd,
                        uint32_t offset,
                        uint32_t bytes_used) {
  return static_cast<arc::VdaContext*>(ctx)->Decode(
      bitstream_id, base::ScopedFD(fd), offset, bytes_used);
}

vda_result_t vda_set_output_buffer_count(void* ctx, size_t num_output_buffers) {
  return static_cast<arc::VdaContext*>(ctx)->SetOutputBufferCount(
      num_output_buffers);
}

vda_result_t vda_use_output_buffer(void* ctx,
                                   int32_t picture_buffer_id,
                                   vda_pixel_format_t format,
                                   int fd,
                                   size_t num_planes,
                                   video_frame_plane_t* planes) {
  return static_cast<arc::VdaContext*>(ctx)->UseOutputBuffer(
      picture_buffer_id, format, base::ScopedFD(fd), num_planes, planes);
}

vda_result_t vda_reuse_output_buffer(void* ctx, int32_t picture_buffer_id) {
  return static_cast<arc::VdaContext*>(ctx)->ReuseOutputBuffer(
      picture_buffer_id);
}

vda_result vda_reset(void* ctx) {
  return static_cast<arc::VdaContext*>(ctx)->Reset();
}

vda_result vda_flush(void* ctx) {
  return static_cast<arc::VdaContext*>(ctx)->Flush();
}
