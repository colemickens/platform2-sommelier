// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/power_manager.h"

#include <chromeos/dbus/service_constants.h>
#include <google/protobuf/message_lite.h>

#include "power_manager/proto_bindings/suspend.pb.h"
#include "wimax_manager/dbus_control.h"
#include "wimax_manager/manager.h"
#include "wimax_manager/power_manager_dbus_proxy.h"

using std::string;
using std::vector;

namespace wimax_manager {

namespace {

const uint32 kDefaultSuspendDelayInMilliSeconds = 5000;  // 5s
const uint32 kSuspendTimeoutInSeconds = 15;  // 15s
const char kPowerStateMem[] = "mem";
const char kPowerStateOn[] = "on";
const char kSuspendDelayDescription[] = "wimax-manager";

// Serializes |protobuf| to |out| and returns true on success.
bool SerializeProtocolBuffer(const google::protobuf::MessageLite &protobuf,
                             vector<uint8> *out) {
  CHECK(out);

  out->clear();
  string serialized_protobuf;
  if (!protobuf.SerializeToString(&serialized_protobuf))
    return false;

  out->assign(serialized_protobuf.begin(), serialized_protobuf.end());
  return true;
}

// Deserializes |serialized_protobuf| to |protobuf_out| and returns true on
// success.
bool DeserializeProtocolBuffer(const vector<uint8> &serialized_protobuf,
                               google::protobuf::MessageLite *protobuf_out) {
  CHECK(protobuf_out);

  if (serialized_protobuf.empty())
    return false;

  return protobuf_out->ParseFromArray(&serialized_protobuf.front(),
                                      serialized_protobuf.size());
}

}  // namespace

PowerManager::PowerManager(Manager *wimax_manager)
    : suspend_delay_registered_(false),
      suspend_delay_id_(0),
      suspended_(false),
      wimax_manager_(wimax_manager) {
  CHECK(wimax_manager_);
}

PowerManager::~PowerManager() {
  Finalize();
}

void PowerManager::Initialize() {
  // TODO(benchan): May need to check if power manager is running and defer
  // the invocation of RegisterSuspendDelay when necessary.
  RegisterSuspendDelay(
      base::TimeDelta::FromMilliseconds(kDefaultSuspendDelayInMilliSeconds),
      kSuspendDelayDescription);
}

void PowerManager::Finalize() {
  suspend_timeout_timer_.Stop();
  UnregisterSuspendDelay();
}

void PowerManager::ResumeOnSuspendTimedOut() {
  LOG(WARNING) << "Timed out waiting for power state change signal from "
               << "power manager. Assume suspend is canceled.";
  OnPowerStateChanged(kPowerStateOn);
}

void PowerManager::RegisterSuspendDelay(base::TimeDelta timeout,
                                        const string &description) {
  if (!dbus_proxy())
    return;

  LOG(INFO) << "Register suspend delay of " << timeout.InMilliseconds()
            << " ms.";
  power_manager::RegisterSuspendDelayRequest request_proto;
  request_proto.set_timeout(timeout.ToInternalValue());
  request_proto.set_description(description);
  vector<uint8> serialized_request;
  CHECK(SerializeProtocolBuffer(request_proto, &serialized_request));

  vector<uint8> serialized_reply;
  try {
    serialized_reply = dbus_proxy()->RegisterSuspendDelay(serialized_request);
  } catch (const DBus::Error &error) {
    LOG(ERROR) << "Failed to register suspend delay. DBus exception: "
               << error.name() << ": " << error.what();
    return;
  }

  power_manager::RegisterSuspendDelayReply reply_proto;
  if (!DeserializeProtocolBuffer(serialized_reply, &reply_proto)) {
    LOG(ERROR) << "Failed to register suspend delay.  Couldn't parse response.";
    return;
  }
  suspend_delay_registered_ = true;
  suspend_delay_id_ = reply_proto.delay_id();
}

void PowerManager::UnregisterSuspendDelay() {
  if (!suspend_delay_registered_)
    return;

  if (!dbus_proxy()) {
    suspend_delay_registered_ = false;
    return;
  }

  LOG(INFO) << "Calling UnregisterSuspendDelay (" << suspend_delay_id_ << ")";
  power_manager::UnregisterSuspendDelayRequest request_proto;
  request_proto.set_delay_id(suspend_delay_id_);
  vector<uint8> serialized_request;
  CHECK(SerializeProtocolBuffer(request_proto, &serialized_request));

  try {
    dbus_proxy()->UnregisterSuspendDelay(serialized_request);
    suspend_delay_registered_ = false;
    suspend_delay_id_ = 0;
  } catch (const DBus::Error &error) {
    LOG(ERROR) << "Failed to unregister suspend delay. DBus exception: "
               << error.name() << ": " << error.what();
  }
}

void PowerManager::OnSuspendImminent(const vector<uint8> &serialized_proto) {
  power_manager::SuspendImminent proto;
  if (!DeserializeProtocolBuffer(serialized_proto, &proto)) {
    LOG(ERROR) << "Failed to parse SuspendImminent signal.";
    return;
  }

  LOG(INFO) << "Received SuspendImminent (" << proto.suspend_id() << ").";
  if (!suspended_) {
    wimax_manager_->Suspend();
    suspended_ = true;
  }
  SendHandleSuspendReadiness(proto.suspend_id());
  // If the power manager does not emit a PowerStateChanged "mem" signal within
  // |kSuspendTimeoutInSeconds|, assume suspend is canceled. Schedule a callback
  // to resume.
  suspend_timeout_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kSuspendTimeoutInSeconds),
      this,
      &PowerManager::ResumeOnSuspendTimedOut);
}

void PowerManager::OnPowerStateChanged(const string &new_power_state) {
  LOG(INFO) << "Power state changed to '" << new_power_state << "'.";

  // Cancel any pending suspend timeout regardless of the new power state
  // to avoid resuming unexpectedly.
  suspend_timeout_timer_.Stop();

  if (new_power_state == kPowerStateMem) {
    suspended_ = true;
    return;
  }

  if (suspended_ && new_power_state == kPowerStateOn) {
    wimax_manager_->Resume();
    suspended_ = false;
  }
}

void PowerManager::SendHandleSuspendReadiness(int suspend_id) {
  LOG(INFO) << "Calling HandleSuspendReadiness (" << suspend_id << ").";
  power_manager::SuspendReadinessInfo proto;
  proto.set_delay_id(suspend_delay_id_);
  proto.set_suspend_id(suspend_id);
  vector<uint8> serialized_proto;
  CHECK(SerializeProtocolBuffer(proto, &serialized_proto));

  try {
    dbus_proxy()->HandleSuspendReadiness(serialized_proto);
  } catch (const DBus::Error &error) {
    LOG(ERROR) << "Failed to call HandleSuspendReadiness. DBus exception: "
               << error.name() << ": " << error.what();
  }
}

}  // namespace wimax_manager
