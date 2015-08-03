// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_EXPORT_H_
#define LIBWEAVE_INCLUDE_WEAVE_EXPORT_H_

#define LIBWEAVE_EXPORT __attribute__((__visibility__("default")))
#define LIBWEAVE_PRIVATE __attribute__((__visibility__("hidden")))

#endif  // LIBWEAVE_INCLUDE_WEAVE_EXPORT_H_
