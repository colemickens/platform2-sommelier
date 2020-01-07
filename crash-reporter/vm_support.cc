// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/vm_support.h"

#include "base/files/file_path.h"
#include "base/no_destructor.h"

#if USE_KVM_GUEST
#include "crash-reporter/vm_support_proper.h"
#endif  // USE_KVM_GUEST

VmSupport::~VmSupport() = default;

VmSupport* VmSupport::Get() {
#if USE_KVM_GUEST
  static base::NoDestructor<VmSupportProper> instance;
  return instance.get();
#else
  return nullptr;
#endif  // USE_KVM_GUEST
}
