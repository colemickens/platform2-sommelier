/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common/jpeg/jpeg_encode_accelerator_impl.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/posix/eintr_wrapper.h>
#include <base/run_loop.h>
#include <mojo/public/c/system/buffer.h>
#include <mojo/public/cpp/system/buffer.h>

#include "cros-camera/common.h"
#include "cros-camera/future.h"
#include "cros-camera/ipc_util.h"

#define STATIC_ASSERT_ENUM(name)                                 \
  static_assert(static_cast<int>(JpegEncodeAccelerator::name) == \
                static_cast<int>(mojom::EncodeStatus::name),     \
                "mismatching enum: " #name)

namespace cros {

STATIC_ASSERT_ENUM(ENCODE_OK);
STATIC_ASSERT_ENUM(HW_JPEG_ENCODE_NOT_SUPPORTED);
STATIC_ASSERT_ENUM(THREAD_CREATION_FAILED);
STATIC_ASSERT_ENUM(INVALID_ARGUMENT);
STATIC_ASSERT_ENUM(INACCESSIBLE_OUTPUT_BUFFER);
STATIC_ASSERT_ENUM(PARSE_IMAGE_FAILED);
STATIC_ASSERT_ENUM(PLATFORM_FAILURE);

std::unique_ptr<JpegEncodeAccelerator> JpegEncodeAccelerator::CreateInstance() {
  return base::WrapUnique<JpegEncodeAccelerator>(
      new JpegEncodeAcceleratorImpl());
}

JpegEncodeAcceleratorImpl::JpegEncodeAcceleratorImpl()
    : ipc_thread_("JeaIpcThread"), buffer_id_(0) {
  VLOGF_ENTER();

  mojo_channel_manager_ = CameraMojoChannelManager::CreateInstance();
}

JpegEncodeAcceleratorImpl::~JpegEncodeAcceleratorImpl() {
  VLOGF_ENTER();
  if (ipc_thread_.IsRunning()) {
    ipc_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&JpegEncodeAcceleratorImpl::DestroyOnIpcThread,
                              base::Unretained(this)));
    ipc_thread_.Stop();
  }
  VLOGF_EXIT();
}

bool JpegEncodeAcceleratorImpl::Start() {
  VLOGF_ENTER();

  if (!ipc_thread_.IsRunning()) {
    if (!ipc_thread_.Start()) {
      LOGF(ERROR) << "Failed to start IPC thread";
      return false;
    }
  }

  auto is_initialized = Future<bool>::Create(nullptr);
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&JpegEncodeAcceleratorImpl::InitializeOnIpcThread,
                            base::Unretained(this),
                            GetFutureCallback(is_initialized)));
  is_initialized->Wait();

  VLOGF_EXIT();

  return is_initialized->Get();
}

void JpegEncodeAcceleratorImpl::InitializeOnIpcThread(
    base::Callback<void(bool)> callback) {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();

  if (jea_ptr_.is_bound()) {
    callback.Run(true);
    return;
  }

  auto request = mojo::GetProxy(&jea_ptr_);

  mojo_channel_manager_->CreateJpegEncodeAccelerator(std::move(request));
  jea_ptr_.set_connection_error_handler(
      base::Bind(&JpegEncodeAcceleratorImpl::OnJpegEncodeAcceleratorError,
                 base::Unretained(this)));

  jea_ptr_->Initialize(callback);
  VLOGF_EXIT();
}

void JpegEncodeAcceleratorImpl::DestroyOnIpcThread() {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();
  jea_ptr_.reset();
  input_shm_map_.clear();
  exif_shm_map_.clear();
  VLOGF_EXIT();
}

void JpegEncodeAcceleratorImpl::OnJpegEncodeAcceleratorError() {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  VLOGF_ENTER();
  LOGF(ERROR) << "There is a mojo error for JpegEncodeAccelerator";
  VLOGF_EXIT();
  DestroyOnIpcThread();
}

int JpegEncodeAcceleratorImpl::EncodeSync(int input_fd,
                                          uint32_t input_buffer_size,
                                          int32_t coded_size_width,
                                          int32_t coded_size_height,
                                          const uint8_t* exif_buffer,
                                          uint32_t exif_buffer_size,
                                          int output_fd,
                                          uint32_t output_buffer_size,
                                          uint32_t* output_data_size) {
  int32_t buffer_id = buffer_id_;
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  buffer_id_ = (buffer_id_ + 1) & 0x3FFFFFFF;

  auto future = Future<int>::Create(nullptr);
  auto callback = base::Bind(&JpegEncodeAcceleratorImpl::EncodeSyncCallback,
      base::Unretained(this), GetFutureCallback(future),
      output_data_size);

  ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&JpegEncodeAcceleratorImpl::EncodeOnIpcThread,
                 base::Unretained(this), buffer_id, input_fd, input_buffer_size,
                 coded_size_width, coded_size_height, exif_buffer, exif_buffer_size,
                 output_fd, output_buffer_size, std::move(callback)));

  if (!future->Wait()) {
    if (!jea_ptr_.is_bound()) {
      LOGF(WARNING) << "There may be an mojo channel error.";
      return TRY_START_AGAIN;
    }
    LOGF(WARNING) << "There is no encode response from JEA mojo channel.";
    return NO_ENCODE_RESPONSE;
  }
  VLOGF_EXIT();
  return future->Get();
}

void JpegEncodeAcceleratorImpl::EncodeOnIpcThread(int32_t buffer_id,
                                                  int input_fd,
                                                  uint32_t input_buffer_size,
                                                  int32_t coded_size_width,
                                                  int32_t coded_size_height,
                                                  const uint8_t* exif_buffer,
                                                  uint32_t exif_buffer_size,
                                                  int output_fd,
                                                  uint32_t output_buffer_size,
                                                  EncodeWithFDCallback callback) {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(input_shm_map_.count(buffer_id), 0);
  DCHECK_EQ(exif_shm_map_.count(buffer_id), 0);

  if (!jea_ptr_.is_bound()) {
    callback.Run(buffer_id, 0, TRY_START_AGAIN);
  }

  std::unique_ptr<base::SharedMemory> input_shm =
      base::WrapUnique(new base::SharedMemory);
  if (!input_shm->CreateAndMapAnonymous(input_buffer_size)) {
    LOGF(WARNING) << "CreateAndMapAnonymous for input failed, size="
                  << input_buffer_size;
    callback.Run(buffer_id, 0, SHARED_MEMORY_FAIL);
    return;
  }
  // Copy content from input file descriptor to shared memory.
  uint8_t* mmap_buf = static_cast<uint8_t*>(
      mmap(NULL, input_buffer_size, PROT_READ, MAP_SHARED, input_fd, 0));

  if (mmap_buf == MAP_FAILED) {
    LOGF(WARNING) << "MMAP for input_fd:" << input_fd << " Failed.";
    callback.Run(buffer_id, 0, MMAP_FAIL);
    return;
  }

  memcpy(input_shm->memory(), mmap_buf, input_buffer_size);
  munmap(mmap_buf, input_buffer_size);

  // Create SharedMemory for Exif buffer and copy data into it.
  std::unique_ptr<base::SharedMemory> exif_shm =
      base::WrapUnique(new base::SharedMemory);
  if (!exif_shm->CreateAndMapAnonymous(exif_buffer_size)) {
    LOGF(WARNING) << "CreateAndMapAnonymous for exif failed, size="
                  << exif_buffer_size;
    callback.Run(buffer_id, 0, SHARED_MEMORY_FAIL);
    return;
  }
  memcpy(exif_shm->memory(), exif_buffer, exif_buffer_size);

  int dup_input_fd = dup(input_shm->handle().fd);
  int dup_exif_fd = dup(exif_shm->handle().fd);
  int dup_output_fd = dup(output_fd);

  mojo::ScopedHandle input_handle = WrapPlatformHandle(dup_input_fd);
  mojo::ScopedHandle exif_handle = WrapPlatformHandle(dup_exif_fd);
  mojo::ScopedHandle output_handle =
      WrapPlatformHandle(dup_output_fd);

  input_shm_map_[buffer_id] = std::move(input_shm);
  exif_shm_map_[buffer_id] = std::move(exif_shm);

  jea_ptr_->EncodeWithFD(buffer_id, std::move(input_handle), input_buffer_size,
                         coded_size_width, coded_size_height,
                         std::move(exif_handle), exif_buffer_size,
                         std::move(output_handle), output_buffer_size,
                         base::Bind(&JpegEncodeAcceleratorImpl::OnEncodeAck,
                                    base::Unretained(this), callback));
}

void JpegEncodeAcceleratorImpl::EncodeSyncCallback(
    base::Callback<void(int)> callback, uint32_t* output_data_size,
    int32_t buffer_id, uint32_t output_size, int status) {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  *output_data_size = output_size;
  callback.Run(status);
}

void JpegEncodeAcceleratorImpl::OnEncodeAck(EncodeWithFDCallback callback,
                                            int32_t buffer_id,
                                            uint32_t output_size,
                                            mojom::EncodeStatus status) {
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(input_shm_map_.count(buffer_id), 1u);
  DCHECK_EQ(exif_shm_map_.count(buffer_id), 1u);
  input_shm_map_.erase(buffer_id);
  exif_shm_map_.erase(buffer_id);
  callback.Run(buffer_id, output_size, static_cast<int>(status));
}

}  // namespace cros
