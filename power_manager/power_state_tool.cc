// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <inttypes.h>
#include <iostream>
#include <string>

#include "base/logging.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/state_control.h"
#include "power_manager/util_dbus.h"
#include "power_state_control.pb.h"

DEFINE_int64(duration, 0, "Duration.");
DEFINE_int64(request_id, 0, "Request_Id.");
DEFINE_bool(disable_idle_dim, false, "Disable dim on idle");
DEFINE_bool(disable_idle_blank, false, "Disable blank on idle");
DEFINE_bool(disable_idle_suspend, false, "Disable suspend on idle");
DEFINE_bool(disable_lid_suspend, false, "Disable suspend on lid closed");
DEFINE_bool(quiet, false, "Only output the request_id on success");
DEFINE_bool(cancel, false, "Cancel an existing request");

// Turn the request into a protobuff and send over dbus.
bool SendStateOverrideRequest(const power_manager::StateControlInfo* info,
                              int* return_value) {
  PowerStateControl protobuf;
  std::string serialized_proto;

  protobuf.set_request_id(info->request_id);
  protobuf.set_duration(info->duration);
  protobuf.set_disable_idle_dim(info->disable_idle_dim);
  protobuf.set_disable_idle_blank(info->disable_idle_blank);
  protobuf.set_disable_idle_suspend(info->disable_idle_suspend);
  protobuf.set_disable_lid_suspend(info->disable_lid_suspend);

  CHECK(protobuf.SerializeToString(&serialized_proto));
  const char* protobuf_data = serialized_proto.data();
  return power_manager::util::CallMethodInPowerD(
             power_manager::kStateOverrideRequest, protobuf_data,
             serialized_proto.size(), return_value);
}


// A tool to send, extend and cancel commands for selectively
// disabling the powerd state machine.
int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_EQ(argc, 1) << "Unexpected arguments. Try --help";

  g_type_init();

  power_manager::StateControlInfo info;

  if (FLAGS_cancel) {
    CHECK_NE(FLAGS_request_id, 0) << "request_id must be set with --cancel";
    power_manager::util::SendSignalWithIntToPowerD(
        power_manager::kStateOverrideCancel, FLAGS_request_id);
    return(0);
  }
  CHECK_GT(FLAGS_duration, 0) << "duration must be non-zero";
  info.request_id = FLAGS_request_id;
  info.duration = FLAGS_duration;
  info.disable_idle_dim = FLAGS_disable_idle_dim;
  info.disable_idle_blank = FLAGS_disable_idle_blank;
  info.disable_idle_suspend = FLAGS_disable_idle_suspend;
  info.disable_lid_suspend = FLAGS_disable_lid_suspend;
  int ret;
  if (!SendStateOverrideRequest(&info, &ret)) {
    std::cerr << "Error sending request" << std::endl;
    return(-1);
  }
  if (!FLAGS_quiet)
    std::cout << "Success.  request_id: ";

  std::cout << ret << std::endl;
  return 0;
}
