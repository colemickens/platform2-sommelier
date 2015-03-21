// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <protobinder/binder_daemon.h>

#include "soma/common/constants.h"
#include "soma/soma.h"

int main() {
  scoped_ptr<soma::Soma> soma_host(new soma::Soma(base::FilePath("/")));
  protobinder::BinderDaemon daemon(soma::kSomaServiceName, soma_host.Pass());
  return daemon.Run();
}
