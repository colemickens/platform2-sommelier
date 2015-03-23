// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <base/memory/scoped_ptr.h>

#include <protobinder/binder_daemon.h>

#include "germ/germ_host.h"

int main() {
  scoped_ptr<germ::GermHost> host(new germ::GermHost());
  protobinder::BinderDaemon daemon("germ", host.Pass());
  return daemon.Run();
}
