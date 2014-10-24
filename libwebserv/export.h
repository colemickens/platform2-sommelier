// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEBSERV_EXPORT_H_
#define LIBWEBSERV_EXPORT_H_

// See detailed explanation of the purpose of LIBWEBSERV_EXPORT in
// chromeos/chromeos_export.h for similar attribute - CHROMEOS_EXPORT.
#define LIBWEBSERV_EXPORT __attribute__((__visibility__("default")))
#define LIBWEBSERV_PRIVATE __attribute__((__visibility__("hidden")))

#endif  // LIBWEBSERV_EXPORT_H_
