// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "cecservice/cec_fd.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <utility>

#include <base/bind.h>
#include <base/posix/eintr_wrapper.h>

namespace cecservice {

CecFdImpl::CecFdImpl(base::ScopedFD fd, base::ScopedFD epoll_fd)
    : fd_(std::move(fd)), epoll_fd_(std::move(epoll_fd)) {}

CecFdImpl::~CecFdImpl() {
  brillo::MessageLoop::current()->CancelTask(read_taskid_);
  brillo::MessageLoop::current()->CancelTask(priority_taskid_);
  brillo::MessageLoop::current()->CancelTask(write_taskid_);
}

bool CecFdImpl::SetLogicalAddresses(struct cec_log_addrs* addresses) const {
  if (HANDLE_EINTR(ioctl(fd_.get(), CEC_ADAP_S_LOG_ADDRS, addresses))) {
    PLOG(ERROR) << "Failed to set logical addresses";
    return false;
  }
  return true;
}

bool CecFdImpl::GetLogicalAddresses(struct cec_log_addrs* addresses) const {
  if (HANDLE_EINTR(ioctl(fd_.get(), CEC_ADAP_G_LOG_ADDRS, addresses))) {
    PLOG(ERROR) << "Failed to get logical addresses";
    return false;
  }
  return true;
}

bool CecFdImpl::ReceiveMessage(struct cec_msg* message) const {
  if (HANDLE_EINTR(ioctl(fd_.get(), CEC_RECEIVE, message))) {
    PLOG(ERROR) << "Failed to receive message";
    return false;
  }
  return true;
}

bool CecFdImpl::ReceiveEvent(struct cec_event* event) const {
  if (HANDLE_EINTR(ioctl(fd_.get(), CEC_DQEVENT, event))) {
    PLOG(ERROR) << "Failed to read event";
    return false;
  }
  return true;
}

CecFd::TransmitResult CecFdImpl::TransmitMessage(
    struct cec_msg* message) const {
  if (HANDLE_EINTR(ioctl(fd_.get(), CEC_TRANSMIT, message))) {
    switch (errno) {
      case ENONET:
        return TransmitResult::kNoNet;
      case EWOULDBLOCK:
        return TransmitResult::kWouldBlock;
      case EINVAL:
        return TransmitResult::kInvalidValue;
      default:
        PLOG(ERROR) << "Failed to transmit message";
        return TransmitResult::kError;
    }
  }
  return TransmitResult::kOk;
}

bool CecFdImpl::GetCapabilities(struct cec_caps* capabilities) const {
  if (HANDLE_EINTR(ioctl(fd_.get(), CEC_ADAP_G_CAPS, capabilities))) {
    PLOG(ERROR) << "Failed to query capabilities";
    return false;
  }
  return true;
}

bool CecFdImpl::SetMode(uint32_t mode) const {
  if (HANDLE_EINTR(ioctl(fd_.get(), CEC_S_MODE, &mode))) {
    PLOG(ERROR) << "Failed to set device mode";
    return false;
  }
  return true;
}

bool CecFdImpl::SetEventCallback(const Callback& callback) {
  DCHECK_EQ(read_taskid_, kTaskIdNull);
  DCHECK_EQ(priority_taskid_, kTaskIdNull);

  callback_ = callback;

  priority_taskid_ = brillo::MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE, epoll_fd_.get(), brillo::MessageLoop::kWatchRead, true,
      base::Bind(&CecFdImpl::OnPriorityDataReady, weak_factory_.GetWeakPtr()));

  read_taskid_ = brillo::MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE, fd_.get(), brillo::MessageLoop::kWatchRead, true,
      base::Bind(&CecFdImpl::OnDataReady, weak_factory_.GetWeakPtr()));

  if (priority_taskid_ == kTaskIdNull || read_taskid_ == kTaskIdNull) {
    LOG_IF(ERROR, priority_taskid_ == kTaskIdNull)
        << "Failed to register watcher for epoll FD read readiness";
    LOG_IF(ERROR, read_taskid_ == kTaskIdNull)
        << "Failed to register watcher for FD read readiness";

    return false;
  }

  return true;
}

bool CecFdImpl::WriteWatch() {
  if (write_taskid_ != kTaskIdNull) {
    return true;
  }

  write_taskid_ = brillo::MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE, fd_.get(), brillo::MessageLoop::kWatchWrite, false,
      base::Bind(&CecFdImpl::OnWriteReady, weak_factory_.GetWeakPtr()));

  if (write_taskid_ == kTaskIdNull) {
    LOG(ERROR) << "Failed to register watcher for FD write readiness";
    return false;
  }
  return true;
}

void CecFdImpl::OnPriorityDataReady() {
  callback_.Run(EventType::kPriorityRead);
}

void CecFdImpl::OnDataReady() {
  callback_.Run(EventType::kRead);
}

void CecFdImpl::OnWriteReady() {
  write_taskid_ = brillo::MessageLoop::kTaskIdNull;
  callback_.Run(EventType::kWrite);
}

CecFdOpenerImpl::CecFdOpenerImpl() = default;

CecFdOpenerImpl::~CecFdOpenerImpl() = default;

std::unique_ptr<CecFd> CecFdOpenerImpl::Open(const base::FilePath& path,
                                             int flags) const {
  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), flags)));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to open: " << path.value();
    return nullptr;
  }

  base::ScopedFD epoll_fd(epoll_create(1));
  if (!epoll_fd.is_valid()) {
    PLOG(ERROR) << "Failed to create epoll descriptor";
    return nullptr;
  }

  epoll_event event;
  event.events = EPOLLPRI;
  if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, fd.get(), &event)) {
    PLOG(ERROR) << "Failed to register device fd on epoll fd";
    return nullptr;
  }

  return std::make_unique<CecFdImpl>(std::move(fd), std::move(epoll_fd));
}

}  // namespace cecservice
