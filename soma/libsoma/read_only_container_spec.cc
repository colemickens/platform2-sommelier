// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/libsoma/read_only_container_spec.h"

#include <base/logging.h>

#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace soma {

ReadOnlyContainerSpec::ReadOnlyContainerSpec(ContainerSpec* spec)
    : internal_(spec) {
  DCHECK(internal_);
}

ReadOnlyContainerSpec::~ReadOnlyContainerSpec() {}

base::FilePath ReadOnlyContainerSpec::service_bundle_path() const {
  return base::FilePath(internal_->service_bundle_path());
}

uid_t ReadOnlyContainerSpec::uid() const { return internal_->uid(); }

gid_t ReadOnlyContainerSpec::gid() const { return internal_->gid(); }

}  // namespace soma
