// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cecservice/cec_device.h"

#include <fcntl.h>
#include <linux/cec-funcs.h>
#include <poll.h>
#include <string.h>

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/stringprintf.h>

namespace cecservice {

CecDeviceImpl::CecDeviceImpl(std::unique_ptr<CecFd> fd) : fd_(std::move(fd)) {}

CecDeviceImpl::~CecDeviceImpl() = default;

bool CecDeviceImpl::Init() {
  if (!fd_->SetEventCallback(
          base::Bind(&CecDeviceImpl::OnFdEvent, weak_factory_.GetWeakPtr()))) {
    fd_.reset();
    return false;
  }
  return true;
}

void CecDeviceImpl::SetStandBy() {
  if (!fd_) {
    LOG(WARNING) << "Device in disabled due to previous errors, ignoring "
                    "standby request.";
    return;
  }

  active_source_ = false;
  if (GetState() == State::kReady) {
    pending_request_ = Request::kStandBy;
    RequestWriteWatch();
  }
}

void CecDeviceImpl::SetWakeUp() {
  if (!fd_) {
    LOG(WARNING) << "Device in disabled due to previous errors, ignoring wake "
                    "up request.";
    return;
  }

  active_source_ = true;
  pending_request_ = Request::kImageViewOn;
  RequestWriteWatch();
}

void CecDeviceImpl::RequestWriteWatch() {
  if (!fd_->WriteWatch()) {
    LOG(ERROR) << "Failed to request write watch on fd, disabling device.";
    fd_.reset();
  }
}

void CecDeviceImpl::RequestWriteWatchIfNeeded() {
  switch (GetState()) {
    case State::kReady:
      if (pending_request_ != Request::kNone || reply_queue_.size()) {
        RequestWriteWatch();
      }
      break;
    case State::kStart:
      if (pending_request_ == Request::kImageViewOn) {
        RequestWriteWatch();
      }
      break;
    case State::kNoLogicalAddress:
      break;
  }
}

CecDeviceImpl::State CecDeviceImpl::GetState() const {
  if (physical_address_ == CEC_PHYS_ADDR_INVALID) {
    return State::kStart;
  }
  if (logical_address_ == CEC_LOG_ADDR_INVALID) {
    return State::kNoLogicalAddress;
  }
  return State::kReady;
}

CecDeviceImpl::State CecDeviceImpl::UpdateState(
    const struct cec_event_state_change& event) {
  physical_address_ = event.phys_addr;
  logical_address_ = CEC_LOG_ADDR_INVALID;

  if (event.log_addr_mask) {
    logical_address_ = ffs(event.log_addr_mask) - 1;
  }

  VLOG(1) << base::StringPrintf(
      "State update, physical address: 0x%x logical address: 0x%x",
      static_cast<uint32_t>(physical_address_),
      static_cast<uint32_t>(logical_address_));

  return GetState();
}

bool CecDeviceImpl::ProcessMessagesLostEvent(
    const struct cec_event_lost_msgs& event) {
  LOG(WARNING) << "Received event lost message, lost " << event.lost_msgs
               << " messages";
  return true;
}

bool CecDeviceImpl::ProcessStateChangeEvent(
    const struct cec_event_state_change& event) {
  bool ret = true;
  switch (UpdateState(event)) {
    case State::kNoLogicalAddress:
      reply_queue_.clear();
      ret = SetLogicalAddress();
      break;
    case State::kStart:
      pending_request_ = Request::kNone;
      reply_queue_.clear();
      break;
    case State::kReady:
      break;
  }
  return ret;
}

bool CecDeviceImpl::ProcessEvents() {
  struct cec_event event;

  if (!fd_->ReceiveEvent(&event)) {
    return false;
  }

  switch (event.event) {
    case CEC_EVENT_LOST_MSGS:
      return ProcessMessagesLostEvent(event.lost_msgs);
      break;
    case CEC_EVENT_STATE_CHANGE:
      return ProcessStateChangeEvent(event.state_change);
    default:
      LOG(WARNING) << base::StringPrintf("Unexpected cec event type: 0x%x",
                                         event.event);
      return true;
  }
}

bool CecDeviceImpl::ProcessRead() {
  struct cec_msg msg;
  if (!fd_->ReceiveMessage(&msg)) {
    return false;
  }

  if (msg.sequence) {
    ProcessSentMessage(msg);
  } else {
    ProcessIncomingMessage(&msg);
  }
  return true;
}

bool CecDeviceImpl::ProcessWrite() {
  switch (pending_request_) {
    case Request::kNone:
      return SendReply();
    case Request::kStandBy:
      return SendStandByRequest();
    case Request::kImageViewOn:
      return SendImageViewOn();
    case Request::kActiveSource:
      return SendActiveSource();
  }
}

void CecDeviceImpl::ProcessSentMessage(const struct cec_msg& msg) {
  if (cec_msg_status_is_ok(&msg)) {
    VLOG(1) << base::StringPrintf("Successfully sent message, opcode: 0x%x",
                                  cec_msg_opcode(&msg));
  } else {
    LOG(WARNING) << base::StringPrintf(
        "Failed to send message, opcode: 0x%x tx_status: 0x%x",
        cec_msg_opcode(&msg), static_cast<uint32_t>(msg.tx_status));
  }
}

void CecDeviceImpl::ProcessIncomingMessage(struct cec_msg* msg) {
  struct cec_msg reply;
  switch (cec_msg_opcode(msg)) {
    case CEC_MSG_REQUEST_ACTIVE_SOURCE:
      VLOG(1) << "Received active source request.";
      if (active_source_) {
        VLOG(1) << "We are active source, will respond.";
        cec_msg_init(&reply, logical_address_, CEC_LOG_ADDR_BROADCAST);
        cec_msg_active_source(&reply, physical_address_);
        reply_queue_.push_back(std::move(reply));
      }
      break;
    case CEC_MSG_ACTIVE_SOURCE:
      VLOG(1) << "Received active source message.";
      if (active_source_) {
        VLOG(1) << "We ceased to be active source.";
        active_source_ = false;
      }
      break;
    case CEC_MSG_GIVE_DEVICE_POWER_STATUS:
      VLOG(1) << "Received give power status message.";
      cec_msg_init(&reply, logical_address_, cec_msg_initiator(msg));
      cec_msg_report_power_status(&reply, CEC_OP_POWER_STATUS_ON);
      reply_queue_.push_back(reply);
      break;
    default:
      VLOG(1) << base::StringPrintf("Received message, opcode: 0x%x",
                                    cec_msg_opcode(msg));
      if (!cec_msg_is_broadcast(msg)) {
        VLOG(1) << "Responding with feature abort.";
        cec_msg_reply_feature_abort(msg, CEC_OP_ABORT_UNRECOGNIZED_OP);
        reply_queue_.push_back(*msg);
      } else {
        VLOG(1) << "Ignoring broadcast message.";
      }
      break;
  }
}

CecFd::TransmitResult CecDeviceImpl::SendMessage(struct cec_msg* msg) {
  VLOG(1) << base::StringPrintf("Transmitting message, opcode:0x%x",
                                cec_msg_opcode(msg));
  return fd_->TransmitMessage(msg);
}

bool CecDeviceImpl::SetLogicalAddress() {
  struct cec_log_addrs addresses = {};

  if (!fd_->GetLogicalAddresses(&addresses)) {
    return false;
  }

  // The address has already been set, so we will reuse it.
  if (addresses.num_log_addrs) {
    return true;
  }

  memset(&addresses, 0, sizeof(addresses));
  addresses.cec_version = CEC_OP_CEC_VERSION_1_4;
  addresses.vendor_id = CEC_VENDOR_ID_NONE;
  addresses.osd_name[0] = 0;
  addresses.num_log_addrs = 1;
  addresses.log_addr_type[0] = CEC_LOG_ADDR_TYPE_PLAYBACK;
  addresses.primary_device_type[0] = CEC_OP_PRIM_DEVTYPE_PLAYBACK;
  addresses.all_device_types[0] = CEC_OP_ALL_DEVTYPE_PLAYBACK;
  addresses.flags = CEC_LOG_ADDRS_FL_ALLOW_UNREG_FALLBACK;

  return fd_->SetLogicalAddresses(&addresses);
}

bool CecDeviceImpl::SendReply() {
  if (reply_queue_.empty()) {
    return true;
  }

  struct cec_msg& msg = reply_queue_.front();
  switch (SendMessage(&msg)) {
    case CecFd::TransmitResult::kOk:
    case CecFd::TransmitResult::kNoNet:
    case CecFd::TransmitResult::kInvalidValue:
      reply_queue_.pop_front();
      return true;
    case CecFd::TransmitResult::kWouldBlock:
      return true;
    case CecFd::TransmitResult::kError:
      LOG(ERROR) << base::StringPrintf(
          "Failed to send response with opcode:0x%x", cec_msg_opcode(&msg));
      return false;
  }

  NOTREACHED() << "Unhandled result";
  return false;
}

void CecDeviceImpl::OnFdEvent(CecFd::EventType event) {
  if (!fd_) {
    return;
  }

  bool ret;
  switch (event) {
    case CecFd::EventType::kPriorityRead:
      ret = ProcessEvents();
      break;
    case CecFd::EventType::kRead:
      ret = ProcessRead();
      break;
    case CecFd::EventType::kWrite:
      ret = ProcessWrite();
      break;
  }

  if (!ret) {
    fd_.reset();
    return;
  }

  RequestWriteWatchIfNeeded();
}

bool CecDeviceImpl::SendStandByRequest() {
  struct cec_msg msg;
  cec_msg_init(&msg, logical_address_, CEC_LOG_ADDR_TV);
  cec_msg_standby(&msg);

  switch (SendMessage(&msg)) {
    case CecFd::TransmitResult::kOk:
    case CecFd::TransmitResult::kNoNet:
    case CecFd::TransmitResult::kInvalidValue:
      pending_request_ = Request::kNone;
      return true;
    case CecFd::TransmitResult::kWouldBlock:
      return true;
    case CecFd::TransmitResult::kError:
      LOG(ERROR) << "Failed to send stand by request.";
      return false;
  }

  NOTREACHED() << "Unhandled result";
  return false;
}

bool CecDeviceImpl::SendImageViewOn() {
  struct cec_msg msg;
  if (GetState() == State::kStart) {
    cec_msg_init(&msg, CEC_LOG_ADDR_UNREGISTERED, CEC_LOG_ADDR_TV);
  } else {
    cec_msg_init(&msg, logical_address_, CEC_LOG_ADDR_TV);
  }
  cec_msg_image_view_on(&msg);

  switch (SendMessage(&msg)) {
    case CecFd::TransmitResult::kOk:
      pending_request_ = Request::kActiveSource;
      return true;
    case CecFd::TransmitResult::kNoNet:
    case CecFd::TransmitResult::kInvalidValue:
      pending_request_ = Request::kNone;
      return true;
    case CecFd::TransmitResult::kWouldBlock:
      return true;
    case CecFd::TransmitResult::kError:
      LOG(ERROR) << "Failed to send image view on request.";
      return false;
  }

  NOTREACHED() << "Unhandled result";
  return false;
}

bool CecDeviceImpl::SendActiveSource() {
  struct cec_msg msg;
  cec_msg_init(&msg, logical_address_, CEC_LOG_ADDR_BROADCAST);
  cec_msg_active_source(&msg, physical_address_);

  switch (SendMessage(&msg)) {
    case CecFd::TransmitResult::kOk:
    case CecFd::TransmitResult::kNoNet:
    case CecFd::TransmitResult::kInvalidValue:
      pending_request_ = Request::kNone;
      return true;
    case CecFd::TransmitResult::kWouldBlock:
      return true;
    case CecFd::TransmitResult::kError:
      LOG(ERROR) << "Failed to send active source broadcast.";
      return false;
  }

  NOTREACHED() << "Unhandled result";
  return false;
}

CecDeviceFactoryImpl::CecDeviceFactoryImpl(const CecFdOpener* cec_fd_opener)
    : cec_fd_opener_(cec_fd_opener) {}

CecDeviceFactoryImpl::~CecDeviceFactoryImpl() = default;

std::unique_ptr<CecDevice> CecDeviceFactoryImpl::Create(
    const base::FilePath& path) const {
  std::unique_ptr<CecFd> fd = cec_fd_opener_->Open(path, O_NONBLOCK);
  if (!fd) {
    return nullptr;
  }

  struct cec_caps caps;
  if (!fd->GetCapabilities(&caps)) {
    return nullptr;
  }

  LOG(INFO) << base::StringPrintf("CEC adapter driver:%s name:%s caps:0x%x",
                                  caps.driver, caps.name, caps.capabilities);

  // At the moment the only adapters supported are the ones that:
  // - handle configuration of physical address on their own (i.e. don't have
  // CEC_CAP_PHYS_ADDR flag set)
  // - allow us to configure logical addrresses (i.e. have CEC_CAP_LOG_ADDRS
  // set)
  if ((caps.capabilities & CEC_CAP_PHYS_ADDR) ||
      !(caps.capabilities & CEC_CAP_LOG_ADDRS)) {
    LOG(WARNING) << "Device does not have required capabilities to function "
                    "with this service";
    return nullptr;
  }

  uint32_t mode = CEC_MODE_EXCL_INITIATOR;
  if (!fd->SetMode(mode)) {
    LOG(ERROR) << "Failed to set an exclusive initiator mode on the device";
    return nullptr;
  }

  auto device = std::make_unique<CecDeviceImpl>(std::move(fd));
  if (!device->Init()) {
    return nullptr;
  }

  return device;
}

}  // namespace cecservice
