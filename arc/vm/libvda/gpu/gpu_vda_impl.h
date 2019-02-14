// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_LIBVDA_GPU_GPU_VDA_IMPL_H_
#define ARC_VM_LIBVDA_GPU_GPU_VDA_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include <base/threading/thread.h>
#include <base/threading/thread_checker.h>

#include "arc/vm/libvda/gpu/mojom/video.mojom.h"
#include "arc/vm/libvda/wrapper.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace arc {

// GpuVdaImpl connects to GpuArcVideoDecodeAccelerator using the LibvdaService
// D-Bus service LibvdaService and Mojo to perform video decoding. Only a single
// instantiated GpuVdaImpl object should exist at a time.
class GpuVdaImpl : public VdaImpl {
 public:
  GpuVdaImpl();
  ~GpuVdaImpl();

  // VdaImpl overrides.
  VdaContext* InitDecodeSession(vda_profile_t profile) override;
  void CloseDecodeSession(VdaContext* context) override;

  // Creates and returns a pointer to a GpuVdaImpl object. This returns a raw
  // pointer instead of a unique_ptr since this will eventually be returned to a
  // C interface. This object should be destroyed with the 'delete' operator
  // when no longer used.
  static GpuVdaImpl* Create();

 private:
  bool Initialize();
  void InitializeOnIpcThread(bool* init_success);
  void InitDecodeSessionOnIpcThread(vda_profile_t profile,
                                    base::WaitableEvent* init_complete_event,
                                    VdaContext** out_context);
  void InitDecodeSessionAfterContextInitializedOnIpcThread(
      base::WaitableEvent* init_complete_event,
      VdaContext** out_context,
      std::unique_ptr<VdaContext> context,
      vda_result_t result);
  void CloseDecodeSessionOnIpcThread(VdaContext* context);
  void CleanupOnIpcThread();
  void OnVdaFactoryError(uint32_t custom_reason,
                         const std::string& description);

  base::Thread ipc_thread_;
  // TODO(alexlau): Use THREAD_CHECKER macro after libchrome uprev
  // (crbug.com/909719).
  base::ThreadChecker ipc_thread_checker_;
  arc::mojom::VideoAcceleratorFactoryPtr vda_factory_ptr_;
};

}  // namespace arc

#endif  // ARC_VM_LIBVDA_GPU_GPU_VDA_IMPL_H_
