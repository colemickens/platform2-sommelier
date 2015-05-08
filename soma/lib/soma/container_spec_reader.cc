// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/container_spec_reader.h"

#include <sys/types.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>

#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace soma {
namespace parser {

ContainerSpecReader::ContainerSpecReader() = default;
ContainerSpecReader::~ContainerSpecReader() = default;

std::unique_ptr<ContainerSpec> ContainerSpecReader::Read(
    const base::FilePath& spec_file) {
  VLOG(1) << "Reading container spec at " << spec_file.value();
  std::string serialized;
  if (!base::ReadFileToString(spec_file, &serialized)) {
    PLOG(ERROR) << "Can't read " << spec_file.value();
    return nullptr;
  }

  std::unique_ptr<ContainerSpec> spec(new ContainerSpec);
  if (!spec->ParseFromString(serialized)) {
    LOG(ERROR) << "ContainerSpec at " << spec_file.value() << " did not parse.";
    return nullptr;
  }

  return std::move(spec);
}

}  // namespace parser
}  // namespace soma
