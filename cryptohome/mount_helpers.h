/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Header file for mount helpers.
 */
#ifndef CRYPTOHOME_MOUNT_HELPERS_H_
#define CRYPTOHOME_MOUNT_HELPERS_H_

#include <glib.h>

/* General utility functions. */
void shred(const char* keyfile);

#endif  // CRYPTOHOME_MOUNT_HELPERS_H_
