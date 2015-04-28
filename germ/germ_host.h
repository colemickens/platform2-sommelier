// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_GERM_HOST_H_
#define GERM_GERM_HOST_H_

#include "base/macros.h"

#include "germ/germ_zygote.h"
#include "germ/launcher.h"
#include "germ/proto_bindings/germ.pb.rpc.h"

namespace germ {

class GermHost : public IGermHostInterface {
 public:
  explicit GermHost(GermZygote* zygote);
  virtual ~GermHost() = default;

  // Implement IGermHostInterface.
  Status Launch(LaunchRequest* request, LaunchResponse* response) override;
  Status Terminate(TerminateRequest* request,
                   TerminateResponse* response) override;

 private:
  GermZygote* zygote_;

  DISALLOW_COPY_AND_ASSIGN(GermHost);
};

}  // namespace germ

#endif  // GERM_GERM_HOST_H_
