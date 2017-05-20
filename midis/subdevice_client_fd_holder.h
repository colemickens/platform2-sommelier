// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_SUBDEVICE_CLIENT_FD_HOLDER_H_
#define MIDIS_SUBDEVICE_CLIENT_FD_HOLDER_H_

#include <memory>

#include <base/files/scoped_file.h>
#include <base/memory/weak_ptr.h>
#include <brillo/message_loops/message_loop.h>

namespace midis {

// Container to store the client_id, pipe FD and file watcher task id for the
// corresponding pipe.
class SubDeviceClientFdHolder {
 public:
  using ClientDataCallback = base::Callback<void(
      uint32_t subdevice_id, const uint8_t* buffer, size_t buf_len)>;

  SubDeviceClientFdHolder(uint32_t client_id, uint32_t subdevice_id,
                          base::ScopedFD fd, ClientDataCallback client_data_cb);
  static std::unique_ptr<SubDeviceClientFdHolder> Create(
      uint32_t client_id, uint32_t subdevice_id, base::ScopedFD fd,
      ClientDataCallback client_data_cb);
  ~SubDeviceClientFdHolder();
  int GetRawFd() { return fd_.get(); }
  uint32_t GetClientId() const { return client_id_; }

 private:
  // This function is used as the callback which is run when a
  // WatchFileDescriptor is set up on the client's fd for this particular
  // subdevice. It reads the data from the client, and in turn invokes
  // client_data_cb_ which writes the data to the device h/w.
  void HandleClientMidiData();
  // Starts the WatchFileDescriptor for the client pipe FD.
  bool StartClientMonitoring();
  void StopClientMonitoring();

  uint32_t client_id_;
  uint32_t subdevice_id_;
  base::ScopedFD fd_;
  brillo::MessageLoop::TaskId pipe_taskid_;
  ClientDataCallback client_data_cb_;
  base::WeakPtrFactory<SubDeviceClientFdHolder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SubDeviceClientFdHolder);
};

}  // namespace midis

#endif  // MIDIS_SUBDEVICE_CLIENT_FD_HOLDER_H_
