// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_MOCK_GERM_ZYGOTE_H_
#define GERM_MOCK_GERM_ZYGOTE_H_

#include "germ/germ_zygote.h"

#include <base/macros.h>
#include <gmock/gmock.h>

#include "germ/proto_bindings/soma_container_spec.pb.h"

namespace germ {

class MockGermZygote : public GermZygote {
 public:
  MockGermZygote() = default;
  ~MockGermZygote() = default;

  MOCK_METHOD2(StartContainer, bool(const soma::ContainerSpec&, pid_t*));
  MOCK_METHOD2(Kill, bool(pid_t, int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGermZygote);
};

}  // namespace germ

#endif  // GERM_MOCK_GERM_ZYGOTE_H_
