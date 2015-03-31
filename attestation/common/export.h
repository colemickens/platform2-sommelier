// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_COMMON_EXPORT_H_
#define ATTESTATION_COMMON_EXPORT_H_

// Use this for any class or function that needs to be exported from
// libattestation.so.
// E.g. ATTESTATION_EXPORT void foo();
#define ATTESTATION_EXPORT __attribute__((__visibility__("default")))

#endif  // ATTESTATION_COMMON_EXPORT_H_
