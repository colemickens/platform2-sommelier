// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_init.h"

#include <sysexits.h>

#include <chromeos/daemons/daemon.h>

#include "germ/process_reaper.h"
#include "germ/proto_bindings/soma_container_spec.pb.h"

namespace germ {

GermInit::GermInit(const soma::ContainerSpec& spec) : spec_(spec) {}
GermInit::~GermInit() {}

int GermInit::OnInit() {
  process_reaper_.RegisterWithDaemon(this);
  return EX_OK;
}

}  // namespace germ
