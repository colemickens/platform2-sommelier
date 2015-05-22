// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/soma.h"

#include <memory>
#include <string>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/logging.h>

#include "soma/proto_bindings/soma.pb.h"
#include "soma/proto_bindings/soma_sandbox_spec.pb.h"

namespace soma {
namespace {
bool IsInvalid(const std::string& endpoint_name) {
  return (endpoint_name.empty() ||
          endpoint_name == base::FilePath::kCurrentDirectory ||
          endpoint_name == base::FilePath::kParentDirectory ||
          endpoint_name.find('/') != std::string::npos);
}

}  // namespace

Soma::Soma(const base::FilePath& bundle_root)
    : root_(bundle_root), reader_(new SandboxSpecReader) {
}

Status Soma::GetSandboxSpec(GetSandboxSpecRequest* request,
                            GetSandboxSpecResponse* response) {
  const std::string& endpoint_name(request->endpoint_name());
  if (IsInvalid(endpoint_name)) {
    return STATUS_APP_ERROR_LOG(
        logging::LOG_WARNING, GetSandboxSpecResponse::INVALID_NAME,
        "Request must contain a valid name, not " + endpoint_name);
  }
  std::unique_ptr<SandboxSpec> spec = reader_->Read(NameToPath(endpoint_name));
  if (spec)
    response->mutable_sandbox_spec()->CheckTypeAndMergeFrom(*spec.get());
  return STATUS_OK();
}

// Running over all spec files in the directory on every call might be
// way too slow. If so, we could do it once at startup and then cache them,
// possibly providing an RPC to make us refresh the cache.
Status Soma::GetPersistentSandboxSpecs(
    GetPersistentSandboxSpecsRequest* ignored,
    GetPersistentSandboxSpecsResponse* response) {
  base::FileEnumerator files(root_, false, base::FileEnumerator::FILES,
                             "*.spec");
  for (base::FilePath spec_path = files.Next(); !spec_path.empty();
       spec_path = files.Next()) {
    std::unique_ptr<SandboxSpec> spec = reader_->Read(spec_path);
    if (spec && spec->is_persistent())
      response->add_sandbox_specs()->CheckTypeAndMergeFrom(*spec.get());
  }
  return STATUS_OK();
}

base::FilePath Soma::NameToPath(const std::string& endpoint_name) const {
  return root_.Append(endpoint_name).ReplaceExtension(".spec");
}

}  // namespace soma
