// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/sandbox_spec_reader.h"

#include <sys/types.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>

#include "soma/proto_bindings/soma_sandbox_spec.pb.h"

namespace soma {

SandboxSpecReader::SandboxSpecReader() = default;
SandboxSpecReader::~SandboxSpecReader() = default;

std::unique_ptr<SandboxSpec> SandboxSpecReader::Read(
    const base::FilePath& spec_file) {
  VLOG(1) << "Reading sandbox spec at " << spec_file.value();
  std::string serialized;
  if (!base::ReadFileToString(spec_file, &serialized)) {
    PLOG(ERROR) << "Can't read " << spec_file.value();
    return nullptr;
  }

  std::unique_ptr<SandboxSpec> spec(new SandboxSpec);
  if (!spec->ParseFromString(serialized)) {
    LOG(ERROR) << "SandboxSpec at " << spec_file.value() << " did not parse.";
    return nullptr;
  }

  return spec;
}

}  // namespace soma
