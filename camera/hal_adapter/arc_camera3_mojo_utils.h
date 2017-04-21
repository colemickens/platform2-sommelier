/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_ARC_CAMERA3_MOJO_UTILS_H_
#define HAL_ADAPTER_ARC_CAMERA3_MOJO_UTILS_H_

#include <map>
#include <memory>
#include <unordered_map>
#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/single_thread_task_runner.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "arc/common.h"
#include "arc/future.h"
#include "hal_adapter/common_types.h"
#include "hal_adapter/mojo/arc_camera3.mojom.h"
#include "hardware/camera3.h"

namespace internal {

// Serialize / deserialize helper functions.

mojo::ScopedHandle WrapPlatformHandle(int handle);

int UnwrapPlatformHandle(mojo::ScopedHandle handle);

// SerializeStreamBuffer is used in CameraDeviceAdapter::ProcessCaptureResult to
// pass a result buffer handle to ARC++.  For the input / output buffers, we do
// not need to serialize the whole native handle but instead we can simply
// return their corresponding handle IDs.  When ARC++ receives the result it
// will restore using the handle ID the original buffer handles which were
// passed down when the frameworks called process_capture_request.
arc::mojom::Camera3StreamBufferPtr SerializeStreamBuffer(
    const camera3_stream_buffer_t* buffer,
    const UniqueStreams& streams,
    const std::unordered_map<uint64_t, std::unique_ptr<camera_buffer_handle_t>>&
        buffer_handles);

int DeserializeStreamBuffer(
    const arc::mojom::Camera3StreamBufferPtr& ptr,
    const UniqueStreams& streams,
    const std::unordered_map<uint64_t, std::unique_ptr<camera_buffer_handle_t>>&
        buffer_handles,
    camera3_stream_buffer_t* buffer);

arc::mojom::CameraMetadataPtr SerializeCameraMetadata(
    const camera_metadata_t* metadata);

internal::CameraMetadataUniquePtr DeserializeCameraMetadata(
    const arc::mojom::CameraMetadataPtr& metadata);
// Template classes for Mojo IPC delegates

// A wrapper around a mojo::InterfacePtr<T>.  This template class represents a
// Mojo communication channel to a remote Mojo binding implementation of T.
template <typename T>
class MojoChannel : public base::SupportsWeakPtr<MojoChannel<T>> {
 public:
  MojoChannel(mojo::InterfacePtrInfo<T> interface_ptr_info,
              scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {
    VLOGF_ENTER();
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MojoChannel<T>::BindOnThread, base::AsWeakPtr(this),
                   base::Passed(&interface_ptr_info)));
  }

  ~MojoChannel() {
    VLOGF_ENTER();
    // We need to wait for ResetInterfacePtrOnThread to finish before return
    // otherwise it would cause race condition in destruction of
    // |interface_ptr_| and may CHECK.
    auto future = internal::Future<void>::Create(nullptr);
    if (task_runner_->BelongsToCurrentThread()) {
      ResetInterfacePtrOnThread(internal::GetFutureCallback(future));
    } else {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&MojoChannel<T>::ResetInterfacePtrOnThread,
                                base::AsWeakPtr(this),
                                internal::GetFutureCallback(future)));
    }
    future->Wait();
  }

 protected:
  // All the Mojo communication happens on |task_runner_|.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  mojo::InterfacePtr<T> interface_ptr_;

  // For the future objects in derived class.
  CancellationRelay relay_;

 private:
  void BindOnThread(mojo::InterfacePtrInfo<T> interface_ptr_info) {
    VLOGF_ENTER();
    DCHECK(task_runner_->BelongsToCurrentThread());
    interface_ptr_ = mojo::MakeProxy(std::move(interface_ptr_info));
    if (!interface_ptr_.is_bound()) {
      LOGF(ERROR) << "Failed to bind interface_ptr_";
      return;
    }
    interface_ptr_.set_connection_error_handler(base::Bind(
        &MojoChannel<T>::OnIpcConnectionLostOnThread, base::AsWeakPtr(this)));
    interface_ptr_.QueryVersion(base::Bind(
        &MojoChannel<T>::OnQueryVersionOnThread, base::AsWeakPtr(this)));
  }

  void OnIpcConnectionLostOnThread() {
    VLOGF_ENTER();
    DCHECK(task_runner_->BelongsToCurrentThread());
    LOGF(INFO) << "Mojo interface connection lost";
    relay_.CancelAllFutures();
    interface_ptr_.reset();
  }

  void OnQueryVersionOnThread(uint32_t version) {
    VLOGF_ENTER();
    DCHECK(task_runner_->BelongsToCurrentThread());
    LOGF(INFO) << "Bridge ready (version=" << version << ")";
  }

  void ResetInterfacePtrOnThread(const base::Closure& callback) {
    VLOGF_ENTER();
    DCHECK(task_runner_->BelongsToCurrentThread());
    interface_ptr_.reset();
    callback.Run();
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(MojoChannel);
};

// A wrapper around a mojo::Binding<T>.  This template class represents an
// implementation of Mojo interface T.
template <typename T>
class MojoBinding : public T {
 public:
  MojoBinding(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
              base::Closure quit_cb = base::Closure())
      : task_runner_(task_runner),
        quit_cb_(quit_cb),
        binding_(this),
        weak_ptr_factory_(this) {
    VLOGF_ENTER();
  }

  ~MojoBinding() {
    VLOGF_ENTER();
    // We need to wait for CloseBindingOnThread to finish before return
    // otherwise it would cause race condition in destruction of |binding_| and
    // may CHECK.
    auto future = internal::Future<void>::Create(nullptr);
    if (task_runner_->BelongsToCurrentThread()) {
      CloseBindingOnThread(internal::GetFutureCallback(future));
    } else {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&MojoBinding<T>::CloseBindingOnThread,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        internal::GetFutureCallback(future)));
    }
    future->Wait();
  }

  mojo::InterfacePtr<T> CreateInterfacePtr() {
    VLOGF_ENTER();
    auto future = internal::Future<mojo::InterfacePtr<T>>::Create(nullptr);
    if (task_runner_->BelongsToCurrentThread()) {
      CreateInterfacePtrOnThread(internal::GetFutureCallback(future));
    } else {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&MojoBinding<T>::CreateInterfacePtrOnThread,
                                weak_ptr_factory_.GetWeakPtr(),
                                internal::GetFutureCallback(future)));
    }
    return future->Get();
  }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    VLOGF_ENTER();
    task_runner_->PostTask(FROM_HERE, base::Bind(&MojoBinding<T>::BindOnThread,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 base::Passed(&handle)));
  }

 protected:
  // All the methods of T that this class implements run on |task_runner_|.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

 private:
  void CloseBindingOnThread(const base::Closure& callback) {
    VLOGF_ENTER();
    DCHECK(task_runner_->BelongsToCurrentThread());
    if (binding_.is_bound()) {
      binding_.Close();
    }
    if (!quit_cb_.is_null()) {
      quit_cb_.Run();
    }
    callback.Run();
  }

  void CreateInterfacePtrOnThread(
      const base::Callback<void(mojo::InterfacePtr<T>)>& cb) {
    // Call CreateInterfacePtrAndBind() on thread_ to serve the RPC.
    VLOGF_ENTER();
    DCHECK(task_runner_->BelongsToCurrentThread());
    mojo::InterfacePtr<T> interfacePtr = binding_.CreateInterfacePtrAndBind();
    binding_.set_connection_error_handler(
        base::Bind(&MojoBinding<T>::OnChannelClosedOnThread,
                   weak_ptr_factory_.GetWeakPtr()));
    cb.Run(std::move(interfacePtr));
  }

  void BindOnThread(mojo::ScopedMessagePipeHandle handle) {
    VLOGF_ENTER();
    DCHECK(task_runner_->BelongsToCurrentThread());
    binding_.Bind(std::move(handle));
    binding_.set_connection_error_handler(
        base::Bind(&MojoBinding<T>::OnChannelClosedOnThread,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnChannelClosedOnThread() {
    VLOGF_ENTER();
    DCHECK(task_runner_->BelongsToCurrentThread());
    LOGF(INFO) << "Mojo binding channel closed";
    if (binding_.is_bound()) {
      binding_.Close();
    }
    if (!quit_cb_.is_null()) {
      quit_cb_.Run();
    }
  }

  base::Closure quit_cb_;

  mojo::Binding<T> binding_;

  base::WeakPtrFactory<MojoBinding<T>> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MojoBinding);
};

}  // namespace internal

#endif  // HAL_ADAPTER_ARC_CAMERA3_MOJO_UTILS_H_
