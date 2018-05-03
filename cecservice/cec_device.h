// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CECSERVICE_CEC_DEVICE_H_
#define CECSERVICE_CEC_DEVICE_H_

#include <linux/cec.h>

#include <deque>
#include <list>
#include <memory>
#include <string>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <base/files/file_path.h>
#include <chromeos/dbus/service_constants.h>

#include "cecservice/cec_fd.h"

namespace cecservice {

// Object handling interaction with a single /dev/cec* node.
class CecDevice {
 public:
  using GetTvPowerStatusCallback = base::Callback<void(TvPowerStatus)>;

  virtual ~CecDevice() = default;

  // Gets power state of TV.
  virtual void GetTvPowerStatus(GetTvPowerStatusCallback callback) = 0;
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
  // failed and object is unusable.
  bool Init();

  // CecDevice overrides:
  void GetTvPowerStatus(GetTvPowerStatusCallback callback) override;
  void SetStandBy() override;
  void SetWakeUp() override;

 private:
  // Represents CEC adapter state.
  enum class State {
    kStart,  // State when no physical address is known, in this state we
             // are only allowed to send image view on message.
    kNoLogicalAddress,  // The physical address is known but the logical
                        // address is not (yet) configured.
    kReady  // All is set up, we are free to send any type of messages.
  };

  // Represents request that either is to be sent or already has been sent
  // but we didn't yet get response to.
  struct RequestInFlight {
    // The callback to invoke when request completes.
    GetTvPowerStatusCallback callback;

    // Message id assigned by CEC API or 0 if the request has not been sent yet.
    uint32_t sequence_id;
  };

  // Schedules watching for write readiness on the fd if there are some
  // outgoing messages.
  void RequestWriteWatch();

  // Returns the current state.
  State GetState() const;

  // Updates the state based on the event received from CEC core, returns new
  // state.
  State UpdateState(const struct cec_event_state_change& event);

  // Processes messages lost event from CEC, just logs number of lost events
  // and always returns true.
  bool ProcessMessagesLostEvent(const struct cec_event_lost_msgs& event);

  // Acts on process update event from CEC core. If this method returns false
  // then an unexpected error was encountered and the object should be disabled.
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

  // Processes response received to get power status request. Returns false if
  // the message is not a response to a previously sent request.
  bool ProcessPowerStatusResponse(const struct cec_msg& msg);

  // Handles sent message notifications and responses to get tv power queries.
  void ProcessSentMessage(const struct cec_msg& msg);

  // Handles messages directed to us.
  void ProcessIncomingMessage(struct cec_msg* msg);

  // Sends provided message.
  CecFd::TransmitResult SendMessage(struct cec_msg* msg);

  // Sets logical address on the adapter (if it has not been yet configured),
  // returns false if the operation failed.
  bool SetLogicalAddress();

  // Handles fd event.
  void OnFdEvent(CecFd::EventType event);

  // Immediately responds to all currently ongoing queries.
  void RespondToAllPendingQueries(TvPowerStatus response);

  // Disables device.
  void DisableDevice();

  // Current physical address.
  uint16_t physical_address_ = CEC_PHYS_ADDR_INVALID;

  // Current logical address.
  uint8_t logical_address_ = CEC_LOG_ADDR_INVALID;

  // Queue of messages we are about to send.
  std::deque<struct cec_msg> message_queue_;

  // Queue of requests that are in flight.
  std::list<RequestInFlight> requests_;

  // Flag indicating if we believe we are the active source.
  bool active_source_ = false;

  // If true, we should send out an active source message when the bus becomes
  // ready.
  bool pending_active_source_broadcast_ = false;

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
