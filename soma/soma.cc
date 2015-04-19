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

namespace soma {
namespace {
bool IsInvalid(const std::string& service_name) {
  return (service_name.empty() ||
          service_name == base::FilePath::kCurrentDirectory ||
          service_name == base::FilePath::kParentDirectory ||
          service_name.find('/') != std::string::npos);
}

}  // namespace

Soma::Soma(const base::FilePath& bundle_root)
    : root_(bundle_root), reader_(new parser::ContainerSpecReader) {
}

Status Soma::GetContainerSpec(GetContainerSpecRequest* request,
                              GetContainerSpecResponse* response) {
  const std::string& service_name(request->service_name());
  if (IsInvalid(service_name)) {
    return STATUS_APP_ERROR_LOG(
        logging::LOG_WARNING, GetContainerSpecResponse::INVALID_NAME,
        "Request must contain a valid name, not " + service_name);
  }
  std::unique_ptr<ContainerSpec> spec = reader_->Read(NameToPath(service_name));
  if (spec)
    response->mutable_container_spec()->CheckTypeAndMergeFrom(*spec.get());
  return STATUS_OK();
}

// Running over all JSON files in the directory on every call might be
// way too slow. If so, we could do it once at startup and then cache them,
// possibly providing an RPC to make us refresh the cache.
Status Soma::GetPersistentContainerSpecs(
    GetPersistentContainerSpecsRequest* ignored,
    GetPersistentContainerSpecsResponse* response) {
  base::FileEnumerator files(root_, false, base::FileEnumerator::FILES,
                             "*.json");
  for (base::FilePath spec_path = files.Next(); !spec_path.empty();
       spec_path = files.Next()) {
    std::unique_ptr<ContainerSpec> spec = reader_->Read(spec_path);
    if (spec && spec->is_persistent())
      response->add_container_specs()->CheckTypeAndMergeFrom(*spec.get());
  }
  return STATUS_OK();
}

base::FilePath Soma::NameToPath(const std::string& service_name) const {
  return root_.Append(service_name).ReplaceExtension(".json");
}

}  // namespace soma
