// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connection_tester.h"

#include <string>

#include <base/bind.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/connection.h"
#include "shill/connectivity_trial.h"
#include "shill/logging.h"

using base::Bind;
using base::Callback;
using base::StringPrintf;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kPortal;
static string ObjectID(Connection* c) { return c->interface_name(); }
}

const int ConnectionTester::kTrialTimeoutSeconds = 5;

ConnectionTester::ConnectionTester(
    ConnectionRefPtr connection,
    EventDispatcher* dispatcher,
    const Callback<void()>& callback)
    : connection_(connection),
      dispatcher_(dispatcher),
      weak_ptr_factory_(this),
      tester_callback_(callback),
      connectivity_trial_(
          new ConnectivityTrial(connection_,
                                dispatcher_,
                                kTrialTimeoutSeconds,
                                Bind(&ConnectionTester::CompleteTest,
                                     weak_ptr_factory_.GetWeakPtr()))) { }

ConnectionTester::~ConnectionTester() {
  Stop();
  connectivity_trial_.reset();
}

void ConnectionTester::Start() {
  SLOG(connection_.get(), 3) << "In " << __func__;
  if (!connectivity_trial_->Start(ConnectivityTrial::kDefaultURL, 0))
    LOG(ERROR) << StringPrintf("ConnectivityTrial failed to parse default "
                               "URL %s", ConnectivityTrial::kDefaultURL);
}

void ConnectionTester::Stop() {
  SLOG(connection_.get(), 3) << "In " << __func__;
  connectivity_trial_->Stop();
}

void ConnectionTester::CompleteTest(ConnectivityTrial::Result result) {
  LOG(INFO) << StringPrintf("ConnectivityTester completed with phase==%s, "
                            "status==%s",
                            ConnectivityTrial::PhaseToString(
                                result.phase).c_str(),
                            ConnectivityTrial::StatusToString(
                                result.status).c_str());
  Stop();
  tester_callback_.Run();
}

}  // namespace shill

