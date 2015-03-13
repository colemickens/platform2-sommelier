// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singleton class to manage the connection to /dev/binder.
// All interactions with the binder driver are implemented
// by this class.

#ifndef LIBPROTOBINDER_BINDER_MANAGER_H_
#define LIBPROTOBINDER_BINDER_MANAGER_H_

#include <stdint.h>
#include <stdlib.h>

#include "libprotobinder/binder_export.h"
#include "libprotobinder/parcel.h"

namespace protobinder {

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
  bool WriteCmd(void* data, size_t len);
  int WaitAndActionReply(Parcel* reply);

  static void ReleaseBinderBuffer(Parcel* parcel,
                                  const uint8_t* data,
                                  size_t data_size,
                                  const binder_size_t* objects,
                                  size_t objects_size,
                                  void* cookie);
  bool DoBinderReadWriteIoctl(bool do_read);
  int SetUpTransaction(bool is_reply,
                       uint32_t handle,
                       uint32_t code,
                       const Parcel& data,
                       uint32_t flags);
  // Process a single command from binder.
  int ProcessCommand(uint32_t cmd);
  bool GetNextCommandAndProcess();
  int SendReply(const Parcel& reply, int error_code);

  int binder_fd_;
  void* binder_mapped_address_;

  // These parcels are used to pass binder ioctl commands to binder.
  // They carry binder command buffers, not to be confused with Parcels
  // used in Transactions which carry user data.
  Parcel out_commands_;
  Parcel in_commands_;
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_MANAGER_H_
