// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_LIB_PSYCHE_PSYCHE_DAEMON_H_
#define PSYCHE_LIB_PSYCHE_PSYCHE_DAEMON_H_

#include <memory>

#include <base/macros.h>
#include <chromeos/daemons/daemon.h>
#include <psyche/psyche_export.h>

namespace protobinder {
class BinderWatcher;
}  // namespace protobinder

namespace psyche {

class PsycheConnection;

// Base class for daemons that communicate with psyched.
class PSYCHE_EXPORT PsycheDaemon : public chromeos::Daemon {
 public:
  PsycheDaemon();
  ~PsycheDaemon() override;

  PsycheConnection* psyche_connection() {
    return psyche_connection_.get();
  }

 protected:
  // chromeos::Daemon:
  int OnInit() override;

 private:
  std::unique_ptr<protobinder::BinderWatcher> binder_watcher_;
  std::unique_ptr<PsycheConnection> psyche_connection_;

  DISALLOW_COPY_AND_ASSIGN(PsycheDaemon);
};

}  // namespace psyche

#endif  // PSYCHE_LIB_PSYCHE_PSYCHE_DAEMON_H_
