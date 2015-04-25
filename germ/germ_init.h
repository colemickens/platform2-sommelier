// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_GERM_INIT_H_
#define GERM_GERM_INIT_H_

#include <base/macros.h>
#include <chromeos/daemons/daemon.h>

#include "germ/init_process_reaper.h"
#include "germ/proto_bindings/soma_container_spec.pb.h"

namespace germ {

// Init process for Germ containers. Responsible for launching processes
// specified in the given ContainerSpec and for reaping children.
class GermInit : public chromeos::Daemon {
 public:
  explicit GermInit(const soma::ContainerSpec& spec);
  ~GermInit();

 private:
  int OnInit() override;
  void StartProcesses();

  InitProcessReaper init_process_reaper_;
  const soma::ContainerSpec& spec_;

  DISALLOW_COPY_AND_ASSIGN(GermInit);
};

}  // namespace germ

#endif  // GERM_GERM_INIT_H_
