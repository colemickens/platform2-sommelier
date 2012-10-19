// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shims/c_ppp.h"

#include "shill/shims/ppp.h"

using shill::shims::PPP;
using std::string;

void PPPInit() {
  PPP().Start();
}

int PPPHasSecret() {
  return 1;
}

int PPPGetSecret(char *username, char *password) {
  string user, pass;
  if (!PPP().GetSecret(&user, &pass)) {
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

void PPPOnConnect(const char *ifname) {
  PPP().OnConnect(ifname);
}

void PPPOnDisconnect() {
  PPP().OnDisconnect();
}

void PPPOnExit(void */*data*/, int /*arg*/) {
  PPP().Stop();
}
