// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/process_factory.h"

namespace apmanager {

namespace {

base::LazyInstance<ProcessFactory> g_process_factory
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ProcessFactory::ProcessFactory() {}
ProcessFactory::~ProcessFactory() {}

ProcessFactory* ProcessFactory::GetInstance() {
  return g_process_factory.Pointer();
}

brillo::Process* ProcessFactory::CreateProcess() {
  return new brillo::ProcessImpl();
}

}  // namespace apmanager
