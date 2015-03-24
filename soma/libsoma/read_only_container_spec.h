// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIBSOMA_READ_ONLY_CONTAINER_SPEC_H_
#define SOMA_LIBSOMA_READ_ONLY_CONTAINER_SPEC_H_

#include <sys/types.h>

#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>

// NB: library headers refer to each other with relative paths, so that includes
// make sense at both build and install time.
#include "soma_export.h"  // NOLINT(build/include)

namespace soma {

class ContainerSpec;

// More friendly read-only wrapper around the ContainerSpec protobuf.
// This class owns an instance of the protobuf and will hand out data from it
// on request.
class SOMA_EXPORT ReadOnlyContainerSpec {
 public:
  explicit ReadOnlyContainerSpec(ContainerSpec* spec);
  virtual ~ReadOnlyContainerSpec();

  base::FilePath service_bundle_path() const;
  uid_t uid() const;
  gid_t gid() const;

 private:
  ContainerSpec* const internal_;

  DISALLOW_COPY_AND_ASSIGN(ReadOnlyContainerSpec);
};

}  // namespace soma
#endif  // SOMA_LIBSOMA_READ_ONLY_CONTAINER_SPEC_H_
