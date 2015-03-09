// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/soma.h"

#include <string>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>

#include "soma/container_spec_wrapper.h"
#include "soma/proto_bindings/soma.pb.h"
#include "soma/spec_reader.h"

namespace soma {

Soma::Soma(const base::FilePath& bundle_root) : root_(bundle_root) {}

int Soma::GetContainerSpec(const GetContainerSpecRequest& request,
                           GetContainerSpecResponse* response) {
  if (!request.has_service_name() || request.service_name().empty()) {
    LOG(WARNING) << "Request must contain a valid name.";
    return 1;
  }
  scoped_ptr<ContainerSpecWrapper> spec =
      reader_.Read(NameToPath(request.service_name()));
  response->mutable_container_spec()->CheckTypeAndMergeFrom(spec->AsProtoBuf());
  return 0;
}

base::FilePath Soma::NameToPath(const std::string& service_name) const {
  return root_.AppendASCII(service_name);
}

}  // namespace soma
