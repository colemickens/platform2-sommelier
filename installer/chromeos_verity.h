/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CHROMEOS_VERITY
#define CHROMEOS_VERITY

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* chromeos_verity
 * This calculated the verity hash-trie for a filesystem and places it
 * immdiately after the FS on the device, and checks that the expected
 * root hash is generated.
 *
 * @alg - algorithm to use. md5, sha1, or sha256
 * @device - block device which contains the fs and will contain hash trie
 * @blocksize - size of block to hash on.  Usually page size - 4k on x86.
 * @salt - ascii string with a salt value to add before calculating each hash
 * @expected - ascii string with the exptected final root hash value
 * @warn - bool indicating whether we should complain if expected doesn't match
 * return - 0 for success, non-zero indicates failure
 *
 */
int chromeos_verity(const char *alg,
                    const char *device,
                    unsigned blocksize,
                    uint64_t fs_blocks,
                    const char *salt,
                    const char *expected,
                    int warn);

#ifdef __cplusplus
}
#endif

#endif /* CHROMEOS_VERITY */
