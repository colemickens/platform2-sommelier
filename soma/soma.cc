// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/soma.h"

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/logging.h>

#include "soma/proto_bindings/soma.pb.h"

namespace soma {
namespace {
bool IsInvalid(const std::string& service_name) {
  return (service_name.empty() ||
          service_name == base::FilePath::kCurrentDirectory ||
          service_name == base::FilePath::kParentDirectory ||
          service_name.find('/') != std::string::npos);
}

}  // namespace

Soma::Soma(const base::FilePath& bundle_root) : root_(bundle_root) {}

int Soma::GetContainerSpec(GetContainerSpecRequest* request,
                           GetContainerSpecResponse* response) {
  const std::string& service_name(request->service_name());
  if (IsInvalid(service_name)) {
    LOG(WARNING) << "Request must contain a valid name, not " << service_name;
    return 1;
  }
  std::unique_ptr<ContainerSpec> spec = reader_.Read(NameToPath(service_name));
  if (spec)
    response->mutable_container_spec()->CheckTypeAndMergeFrom(*spec.get());
  return 0;
}

base::FilePath Soma::NameToPath(const std::string& service_name) const {
  return root_.Append(service_name).ReplaceExtension(".json");
}

}  // namespace soma
