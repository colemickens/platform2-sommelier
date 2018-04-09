// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CECSERVICE_CEC_DEVICE_H_
#define CECSERVICE_CEC_DEVICE_H_

#include <linux/cec.h>

#include <deque>
#include <memory>
#include <string>

#include <base/memory/weak_ptr.h>
#include <base/files/file_path.h>

#include "cecservice/cec_fd.h"

namespace cecservice {

// Object handling interaction with a single /dev/cec* node.
class CecDevice {
 public:
  virtual ~CecDevice() = default;

  // Sends stand by request to a TV.
  virtual void SetStandBy() = 0;
  // Sends wake up (image view on + active source) messages.
  virtual void SetWakeUp() = 0;
};

// Actual implementation of CecDevice.
class CecDeviceImpl : public CecDevice {
 public:
  explicit CecDeviceImpl(std::unique_ptr<CecFd> fd);
  ~CecDeviceImpl() override;

  // Performs object initialization. Returns false if the initialization
  // failed and object is unusuable.
  bool Init();

  // CecDevice overrides:
  void SetStandBy() override;
  void SetWakeUp() override;

 private:
  // Represents CEC adapter state.
  enum class State {
    kStart,  // State when no physical address is known, in this state we are
             // only allowed to send image view on message.
    kNoLogicalAddress,  // The physical address is known but the logical
                        // address is not (yet) configured.
    kReady  // All is set up, we are free to send any type of messages.
  };

  // Represents the request that we intend to send at the earliest opportunity.
  enum class Request {
    kActiveSource,  // Send active source request.
    kImageViewOn,   // Send image view on request.
    kStandBy,       // Send stand by.
    kNone           // No request pending.
  };

  // Schedules watching for write readiness on the fd.
  void RequestWriteWatch();

  // Schedules watching for write readiness on the fd if the current state
  // demands it.
  void RequestWriteWatchIfNeeded();

  // Returns the current state.
  State GetState() const;

  // Updates the state based on the event received from CEC core, returns new
  // state.
  State UpdateState(const struct cec_event_state_change& event);

  // Processes messages lost event from CEC, just logs number of lost events
  // and always returns true.
  bool ProcessMessagesLostEvent(const struct cec_event_lost_msgs& event);

  // Acts on process update event from CEC core. If this method returns false
  // then an unexpected error was encountered and the object should be not acted
  // upon any further.
  bool ProcessStateChangeEvent(const struct cec_event_state_change& event);

  // Processes incoming events. If false is returned, then unexpected failure
  // occurred and the object should be disabled.
  bool ProcessEvents();

  // Attempts to read incoming data from the fd. If false is returned, then
  // unexpected failure occurred and the object should be disabled.
  bool ProcessRead();

  // Attempts to write data to fd. If false is returned, then unexpected failure
  // occurred and the object should be disabled.
  bool ProcessWrite();

  // Processes sent message notifications, nothing is really done about failures
  // atm. We just log them.
  void ProcessSentMessage(const struct cec_msg& msg);

  // Handles messages directed to us.
  void ProcessIncomingMessage(struct cec_msg* msg);

  // Sends provided message,.
  CecFd::TransmitResult SendMessage(struct cec_msg* msg);

  // Sets logical address on an adapter (if it has not been yet configured),
  // returns false if the operation failed.
  bool SetLogicalAddress();

  // Sends a pending reply (if there is any). Returns false if unexpected error
  // occurred.
  bool SendReply();

  // Handles fd event.
  void OnFdEvent(CecFd::EventType event);

  // Sends a stand by request to a TV. Returns false if unexpected error
  // occurred.
  bool SendStandByRequest();

  // Sends a image view on to a TV. Returns false if unexpected error occurred.
  bool SendImageViewOn();

  // Broadcasts an active source message. Returns false if unexpected error
  // occurred.
  bool SendActiveSource();

  // Current physical address.
  uint16_t physical_address_ = CEC_PHYS_ADDR_INVALID;

  // Current logical address.
  uint8_t logical_address_ = CEC_LOG_ADDR_INVALID;

  // Pending request.
  Request pending_request_ = Request::kNone;

  // Queue of responses we are about to send.
  std::deque<struct cec_msg> reply_queue_;

  // Flag indicating if we believe we are the active source.
  bool active_source_ = false;

  // The descriptor associated with the device.
  std::unique_ptr<CecFd> fd_;

  base::WeakPtrFactory<CecDeviceImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CecDeviceImpl);
};

// Factory creating CEC device handlers.
class CecDeviceFactory {
 public:
  virtual ~CecDeviceFactory() = default;

  // Creates a new CEC device node handler from a given path. Returns empty ptr
  // on failure.
  virtual std::unique_ptr<CecDevice> Create(
      const base::FilePath& path) const = 0;
};

// Concrete implementation of the CEC device handlers factory.
class CecDeviceFactoryImpl : public CecDeviceFactory {
 public:
  explicit CecDeviceFactoryImpl(const CecFdOpener* cec_fd_opener);
  ~CecDeviceFactoryImpl() override;

  // CecDeviceFactory overrides.
  std::unique_ptr<CecDevice> Create(const base::FilePath& path) const override;

 private:
  const CecFdOpener* cec_fd_opener_;

  DISALLOW_COPY_AND_ASSIGN(CecDeviceFactoryImpl);
};

}  // namespace cecservice

#endif  // CECSERVICE_CEC_DEVICE_H_
