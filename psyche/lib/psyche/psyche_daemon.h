// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_LIB_PSYCHE_PSYCHE_DAEMON_H_
#define PSYCHE_LIB_PSYCHE_PSYCHE_DAEMON_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <protobinder/binder_daemon.h>
#include <psyche/psyche_connection.h>

namespace psyche {

class PSYCHE_EXPORT PsycheDaemon : public protobinder::BinderDaemon {
 public:
  PsycheDaemon();
  ~PsycheDaemon() override;

  PsycheConnection* psyche_connection() {
    return psyche_connection_.get();
  }

 protected:
  // BinderDaemon:
  int OnInit() override;

 private:
  std::unique_ptr<PsycheConnection> psyche_connection_;

  DISALLOW_COPY_AND_ASSIGN(PsycheDaemon);
};

}  // namespace psyche

#endif  // PSYCHE_LIB_PSYCHE_PSYCHE_DAEMON_H_
