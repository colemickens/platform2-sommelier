// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shims/c_ppp.h"

#include <base/at_exit.h>

#include "shill/shims/ppp.h"

using shill::shims::PPP;
using std::string;

namespace {
base::AtExitManager *g_exit_manager = NULL;  // Cleans up LazyInstances.
}  // namespace

void PPPInit() {
  g_exit_manager = new base::AtExitManager();
  PPP::GetInstance()->Init();
}

int PPPHasSecret() {
  return 1;
}

int PPPGetSecret(char *username, char *password) {
  string user, pass;
  if (!PPP::GetInstance()->GetSecret(&user, &pass)) {
    return -1;
  }
  if (username) {
    strcpy(username, user.c_str());
  }
  if (password) {
    strcpy(password, pass.c_str());
  }
  return 1;
}

void PPPOnAuthenticateStart() {
  PPP::GetInstance()->OnAuthenticateStart();
}

void PPPOnAuthenticateDone() {
  PPP::GetInstance()->OnAuthenticateDone();
}

void PPPOnConnect(const char *ifname) {
  PPP::GetInstance()->OnConnect(ifname);
}

void PPPOnDisconnect() {
  PPP::GetInstance()->OnDisconnect();
}

void PPPOnExit(void */*data*/, int /*arg*/) {
  LOG(INFO) << __func__;
  delete g_exit_manager;
  g_exit_manager = NULL;
}
