// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GERM_GERM_CLIENT_H_
#define GERM_GERM_CLIENT_H_

#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <protobinder/binder_proxy.h>
#include <psyche/psyche_daemon.h>

#include "germ/launcher.h"
#include "germ/proto_bindings/germ.pb.h"
#include "germ/proto_bindings/germ.pb.rpc.h"

namespace germ {

class GermClient : public psyche::PsycheDaemon {
 public:
  GermClient() : weak_ptr_factory_(this) {}
  ~GermClient() override = default;

  int Launch(const std::string& name,
             const std::vector<std::string>& command_line);
  int Terminate(pid_t pid);

 private:
  void ReceiveService(scoped_ptr<BinderProxy> proxy);

  void DoLaunch(const std::string& name,
                const std::vector<std::string>& command_line);
  void DoTerminate(pid_t pid);

  // PsycheDaemon:
  int OnInit() override;

  std::unique_ptr<BinderProxy> proxy_;
  std::unique_ptr<IGerm> germ_;
  base::Closure callback_;

  // Keep this member last.
  base::WeakPtrFactory<GermClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GermClient);
};

}  // namespace germ

#endif  // GERM_GERM_CLIENT_H_
