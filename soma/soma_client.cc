// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/logging.h>
#include <base/files/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/flag_helper.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iservice_manager.h>

#include "soma/libsoma/constants.h"
#include "soma/libsoma/read_only_container_spec.h"
#include "soma/proto_bindings/container_spec.pb.h"
#include "soma/proto_bindings/soma.pb.h"
#include "soma/proto_bindings/soma.pb.rpc.h"

namespace soma {
scoped_ptr<ContainerSpec> GetContainerSpec(const std::string& service_name) {
  scoped_ptr<protobinder::IBinder> proxy(
      GetServiceManager()->GetService(kSomaServiceName));
  scoped_ptr<ISoma> soma(protobinder::BinderToInterface<ISoma>(proxy.get()));
  if (!soma)
    LOG(FATAL) << "Can't GetService(" << kSomaServiceName << ")";
  GetContainerSpecRequest request;
  GetContainerSpecResponse response;
  request.set_service_name(service_name);
  if (soma->GetContainerSpec(&request, &response) != 0)
    return nullptr;
  return make_scoped_ptr<ContainerSpec>(response.release_container_spec());
}

}  // namespace soma

int main(int argc, char* argv[]) {
  DEFINE_string(service_name, "",
                "Name of service for which to fetch a container spec.");
  chromeos::FlagHelper::Init(argc, argv, "Command-line client for somad.");
  scoped_ptr<soma::ContainerSpec> spec =
      soma::GetContainerSpec(FLAGS_service_name);
  soma::ReadOnlyContainerSpec ro_spec(spec.get());
  LOG(INFO) << ro_spec.service_bundle_path().value();
  return 0;
}
