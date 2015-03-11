// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singleton class to manage the connection to /dev/binder.
// All interactions with the binder driver are implemented
// by this class.

#ifndef LIBBRILLOBINDER_BINDER_MANAGER_H_
#define LIBBRILLOBINDER_BINDER_MANAGER_H_

#include <stdlib.h>
#include <stdint.h>

#include "parcel.h"

#define BINDER_EXPORT __attribute__((visibility("default")))

namespace brillobinder {

class BINDER_EXPORT BinderManager {
 public:
  static BinderManager* GetBinderManager();

  BinderManager();
  ~BinderManager();

  int Transact(uint32_t handle,
               uint32_t code,
               const Parcel& data,
               Parcel* reply,
               uint32_t flags);
  void IncWeakHandle(uint32_t handle);
  void DecWeakHandle(uint32_t handle);
  void EnterLoop();
  bool GetFdForPolling(int* fd);
  bool HandleEvent();

 private:
  int binder_fd_;
  void* binder_mapped_address_;
  static const size_t kBinderMappedSize = (1 * 1024 * 1024) - (4096 * 2);

  bool WriteCmd(void* data, size_t len);
  int WaitAndActionReply(Parcel* reply);
  bool ProcessReply(Parcel* reply, bool* have_reply);

  static void ReleaseBinderBuffer(Parcel* parcel,
                                  const uint8_t* data,
                                  size_t dataSize,
                                  const binder_size_t* objects,
                                  size_t objectsSize,
                                  void* cookie);
  bool DoBinderReadWriteIoctl(bool do_read);
  int SetupTransaction(bool is_reply,
                       uint32_t handle,
                       uint32_t code,
                       const Parcel& data,
                       uint32_t flags);
  int ProcessCommand(uint32_t cmd);
  bool GetNextCommandAndProcess();
  int SendReply(const Parcel& reply, int error_code);
  // These parcels are used to pass binder ioctl commands to binder.
  // They carry binder command buffers, not to be confused with Parcels
  // used in Transactions which carry user data.
  Parcel out_commands_;
  Parcel in_commands_;
};

}  // namespace brillobinder

#endif