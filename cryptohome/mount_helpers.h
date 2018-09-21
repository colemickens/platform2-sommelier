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
int runcmd(const gchar* argv[], gchar** output);
void shred(const char* keyfile);

/* Loopback device attach/detach helpers. */
gchar* loop_attach(int fd, const char* name);
int loop_detach(const gchar* loopback);
int loop_detach_name(const char* name);

/* Encrypted device mapper setup/teardown. */
int dm_setup(uint64_t bytes,
             const gchar* encryption_key,
             const char* name,
             const gchar* device,
             const char* path,
             int discard);
int dm_teardown(const gchar* device);
char* dm_get_key(const gchar* device);

/* Filesystem creation. */
int filesystem_build(const char* device,
                     uint64_t block_bytes,
                     uint64_t blocks_min,
                     uint64_t blocks_max);
int filesystem_resize(const char* device, uint64_t blocks, uint64_t blocks_max);

#endif  // CRYPTOHOME_MOUNT_HELPERS_H_
