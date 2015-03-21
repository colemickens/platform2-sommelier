// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/flag_helper.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iservice_manager.h>

#include "soma/common/constants.h"
#include "soma/proto_bindings/soma.pb.h"
#include "soma/proto_bindings/soma.pb.rpc.h"

namespace soma {

int GetContainerSpec(const std::string& service_name) {
  scoped_ptr<IBinder> proxy(GetServiceManager()->GetService(kSomaServiceName));
  scoped_ptr<ISoma> test(protobinder::BinderToInterface<ISoma>(proxy.get()));
  if (!test)
    LOG(FATAL) << "Can't GetService(" << kSomaServiceName << ")";
  GetContainerSpecRequest request;
  GetContainerSpecResponse response;
  request.set_service_name(service_name);
  return test->GetContainerSpec(&request, &response);
}
}  // namespace soma

int main(int argc, char* argv[]) {
  DEFINE_string(service_name, "",
                "Name of service for which to fetch a container spec.");
  chromeos::FlagHelper::Init(argc, argv, "Command-line client for somad.");
  return soma::GetContainerSpec(FLAGS_service_name);
}
