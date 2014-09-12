// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_LIBBUFFET_EXPORT_H_
#define BUFFET_LIBBUFFET_EXPORT_H_

// See detailed explanation of the purpose of LIBBUFFET_EXPORT in
// chromeos/chromeos_export.h for similar attribute - CHROMEOS_EXPORT.
#define LIBBUFFET_EXPORT __attribute__((__visibility__("default")))
#define LIBBUFFET_PRIVATE __attribute__((__visibility__("hidden")))

#endif  // BUFFET_LIBBUFFET_EXPORT_H_
