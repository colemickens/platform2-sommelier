// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_SOMA_H_
#define SOMA_SOMA_H_

#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>

#include "soma/proto_bindings/soma.pb.h"
#include "soma/proto_bindings/soma.pb.rpc.h"
#include "soma/spec_reader.h"

namespace soma {
class Soma : public ISomaHostInterface {
 public:
  explicit Soma(const base::FilePath& bundle_root);
  virtual ~Soma() = default;

  // Implementation of ISomaHostInterface.
  int GetContainerSpec(GetContainerSpecRequest* request,
                       GetContainerSpecResponse* response) override;

 private:
  // TODO(cmasone): make this do the real thing once we grok that.
  base::FilePath NameToPath(const std::string& service_name) const;

  // Path under which to search for service bundles.
  const base::FilePath root_;

  parser::ContainerSpecReader reader_;

  DISALLOW_COPY_AND_ASSIGN(Soma);
};

}  // namespace soma

#endif  // SOMA_SOMA_H_
