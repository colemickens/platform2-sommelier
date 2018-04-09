// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CECSERVICE_CEC_FD_H_
#define CECSERVICE_CEC_FD_H_

#include <linux/cec.h>

#include <memory>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <brillo/message_loops/message_loop.h>

namespace cecservice {

// Wrapper around CEC device file descriptor, interface to enable unit testing.
class CecFd {
 public:
  // Types of events that can occur on the FD.
  enum class EventType {
    kPriorityRead,  // Priority data can be read.
    kRead,          // Regular data available for reading.
    kWrite,         // Data can be written.
  };

  // Callback used to notify about events occurring on the FD.
  using Callback = base::Callback<void(EventType)>;

  // Result of transmit operation.
  enum class TransmitResult {
    kOk,            // Operation succeeded.
    kNoNet,         // Operation failed, ENONET reported.
    kWouldBlock,    // Operation failed, EWOULDBLOCK reported.
    kInvalidValue,  // Operation failed, EINVAL reported.
    kError,         // Operation failed, not recoverable error.
  };

  virtual ~CecFd() = default;

  // Sets logical addresses of the device, returns false if the operation
  // failed. This is just a wrapper around CEC_ADAP_S_LOG_ADDR ioctl.
  virtual bool SetLogicalAddresses(struct cec_log_addrs* addresses) const = 0;

  // Gets logical addresses of the device, returns false if the operation
  // failed. This is just a wrapper around CEC_ADAP_G_LOG_ADDRS ioctl.
  virtual bool GetLogicalAddresses(struct cec_log_addrs* addresses) const = 0;

  // Receives message, returns false if the operation failed. This is just a
  // wrapper around CEC_RECEIVE ioctl.
  virtual bool ReceiveMessage(struct cec_msg* message) const = 0;

  // Receives pending event, returns false if the operation failed. This is just
  // a wrapper around CEC_DQEVENT ioctl.
  virtual bool ReceiveEvent(struct cec_event* event) const = 0;

  // Transmits message, a wrapper around CEC_TRANSMIT ioctl.
  virtual TransmitResult TransmitMessage(struct cec_msg* message) const = 0;

  // Obtains device capabilities, returns false if the operation failed. This is
  // just a wrapper around CEC_ADAP_G_CAPS ioctl.
  virtual bool GetCapabilities(struct cec_caps* capabilities) const = 0;

  // Sets device mode. Returns false if the operation failed, wrapper around
  // CEC_S_MODE ioctl.
  virtual bool SetMode(uint32_t mode) const = 0;

  // Sets a callback to be called when an event occurs on the FD. The
  // callback is always invoked when regular and priority data arrive (events).
  // Also, the callback is called when watching for write readiness has been
  // requested via WriteWatch method. This operation should be only performed
  // once during lifetime of the object. Returns false if the operation fails.
  virtual bool SetEventCallback(const Callback& callback) = 0;

  // Starts watching descriptor for write readiness. It is one off request.
  // Returns false if the operation fails.
  virtual bool WriteWatch() = 0;
};

// Actual implementation of CEC file descriptor.
class CecFdImpl : public CecFd {
 public:
  explicit CecFdImpl(base::ScopedFD fd, base::ScopedFD epoll_fd);
  ~CecFdImpl() override;

  // CecFd overrides:
  bool SetLogicalAddresses(struct cec_log_addrs* addresses) const override;
  bool GetLogicalAddresses(struct cec_log_addrs* addresses) const override;
  bool ReceiveMessage(struct cec_msg* message) const override;
  bool ReceiveEvent(struct cec_event* event) const override;
  TransmitResult TransmitMessage(struct cec_msg* message) const override;
  bool GetCapabilities(struct cec_caps* capabilities) const override;
  bool SetMode(uint32_t mode) const override;
  bool SetEventCallback(const Callback& callback) override;
  bool WriteWatch() override;

 private:
  void OnPriorityDataReady();
  void OnDataReady();
  void OnWriteReady();

  // Actual FD of the opened device.
  base::ScopedFD fd_;
  // Additional epoll FD used to wait for POLLPRI data.
  base::ScopedFD epoll_fd_;

  const brillo::MessageLoop::TaskId kTaskIdNull =
      brillo::MessageLoop::kTaskIdNull;

  brillo::MessageLoop::TaskId priority_taskid_ = kTaskIdNull;
  brillo::MessageLoop::TaskId read_taskid_ = kTaskIdNull;
  brillo::MessageLoop::TaskId write_taskid_ = kTaskIdNull;

  Callback callback_;

  base::WeakPtrFactory<CecFdImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CecFdImpl);
};

// Utility class to open files, for unit testing.
class CecFdOpener {
 public:
  virtual ~CecFdOpener() = default;

  // Open file, flags argument is passed directly to the open(2) syscall.
  virtual std::unique_ptr<CecFd> Open(const base::FilePath& path,
                                      int flags) const = 0;
};

// Actual implementation of CecFdOpener.
class CecFdOpenerImpl : public CecFdOpener {
 public:
  CecFdOpenerImpl();
  ~CecFdOpenerImpl() override;

  // CecFdOpener:
  std::unique_ptr<CecFd> Open(const base::FilePath& path,
                              int flags) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CecFdOpenerImpl);
};

}  // namespace cecservice

#endif  // CECSERVICE_CEC_FD_H_
