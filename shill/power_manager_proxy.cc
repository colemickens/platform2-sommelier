// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/power_manager_proxy.h"

#include <chromeos/dbus/service_constants.h>
#include <google/protobuf/message_lite.h>

#include "shill/logging.h"
#include "power_manager/proto_bindings/suspend.pb.h"

using std::string;
using std::vector;

namespace shill {

namespace {

// Serializes |protobuf| to |out| and returns true on success.
bool SerializeProtocolBuffer(const google::protobuf::MessageLite &protobuf,
                             vector<uint8_t> *out) {
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
bool DeserializeProtocolBuffer(const vector<uint8_t> &serialized_protobuf,
                               google::protobuf::MessageLite *protobuf_out) {
  CHECK(protobuf_out);
  if (serialized_protobuf.empty())
    return false;
  return protobuf_out->ParseFromArray(&serialized_protobuf.front(),
                                      serialized_protobuf.size());
}

}  // namespace

PowerManagerProxy::PowerManagerProxy(PowerManagerProxyDelegate *delegate,
                                     DBus::Connection *connection)
    : proxy_(delegate, connection) {}

PowerManagerProxy::~PowerManagerProxy() {}

bool PowerManagerProxy::RegisterSuspendDelay(base::TimeDelta timeout,
                                             const string &description,
                                             int *delay_id_out) {
  LOG(INFO) << __func__ << "(" << timeout.InMilliseconds() << ")";

  power_manager::RegisterSuspendDelayRequest request_proto;
  request_proto.set_timeout(timeout.ToInternalValue());
  request_proto.set_description(description);
  vector<uint8_t> serialized_request;
  CHECK(SerializeProtocolBuffer(request_proto, &serialized_request));

  vector<uint8_t> serialized_reply;
  try {
    serialized_reply = proxy_.RegisterSuspendDelay(serialized_request);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }

  power_manager::RegisterSuspendDelayReply reply_proto;
  if (!DeserializeProtocolBuffer(serialized_reply, &reply_proto)) {
    LOG(ERROR) << "Failed to register suspend delay.  Couldn't parse response.";
    return false;
  }
  *delay_id_out = reply_proto.delay_id();
  return true;
}

bool PowerManagerProxy::UnregisterSuspendDelay(int delay_id) {
  LOG(INFO) << __func__ << "(" << delay_id << ")";

  power_manager::UnregisterSuspendDelayRequest request_proto;
  request_proto.set_delay_id(delay_id);
  vector<uint8_t> serialized_request;
  CHECK(SerializeProtocolBuffer(request_proto, &serialized_request));

  try {
    proxy_.UnregisterSuspendDelay(serialized_request);
    return true;
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
}

bool PowerManagerProxy::ReportSuspendReadiness(int delay_id, int suspend_id) {
  LOG(INFO) << __func__  << "(" << delay_id << ", " << suspend_id << ")";

  power_manager::SuspendReadinessInfo proto;
  proto.set_delay_id(delay_id);
  proto.set_suspend_id(suspend_id);
  vector<uint8_t> serialized_proto;
  CHECK(SerializeProtocolBuffer(proto, &serialized_proto));

  try {
    proxy_.HandleSuspendReadiness(serialized_proto);
    return true;
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
}

PowerManagerProxy::Proxy::Proxy(PowerManagerProxyDelegate *delegate,
                                DBus::Connection *connection)
    : DBus::ObjectProxy(*connection,
                        power_manager::kPowerManagerServicePath,
                        power_manager::kPowerManagerServiceName),
      delegate_(delegate) {}

PowerManagerProxy::Proxy::~Proxy() {}

void PowerManagerProxy::Proxy::SuspendImminent(
    const vector<uint8_t> &serialized_proto) {
  LOG(INFO) << __func__;

  power_manager::SuspendImminent proto;
  if (!DeserializeProtocolBuffer(serialized_proto, &proto)) {
    LOG(ERROR) << "Failed to parse SuspendImminent signal.";
    return;
  }
  delegate_->OnSuspendImminent(proto.suspend_id());
}

void PowerManagerProxy::Proxy::SuspendDone(
    const vector<uint8_t> &serialized_proto) {
  LOG(INFO) << __func__;
  power_manager::SuspendDone proto;
  if (!DeserializeProtocolBuffer(serialized_proto, &proto)) {
    LOG(ERROR) << "Failed to parse SuspendDone signal.";
    return;
  }
  delegate_->OnSuspendDone(proto.suspend_id());
}

}  // namespace shill
