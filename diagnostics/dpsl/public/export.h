// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DPSL_PUBLIC_EXPORT_H_
#define DIAGNOSTICS_DPSL_PUBLIC_EXPORT_H_

// Use this for any class or function that needs to be exported from
// libdpsl. E.g. DPSL_EXPORT void foo();
#define DPSL_EXPORT __attribute__((__visibility__("default")))

#endif  // DIAGNOSTICS_DPSL_PUBLIC_EXPORT_H_
