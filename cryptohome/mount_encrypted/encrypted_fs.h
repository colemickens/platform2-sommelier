// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOUNT_ENCRYPTED_ENCRYPTED_FS_H_
#define CRYPTOHOME_MOUNT_ENCRYPTED_ENCRYPTED_FS_H_

#include <glib.h>
#include <inttypes.h>
#include <sys/stat.h>

#include <cryptohome/mount_encrypted.h>

#define STATEFUL_MNT "mnt/stateful_partition"
#define ENCRYPTED_MNT STATEFUL_MNT "/encrypted"

namespace cryptohome {

enum bind_dir {
  BIND_SOURCE,
  BIND_DEST,
};

#define str_literal(s) (const_cast<char*>(s))
static struct bind_mount {
  char* src; /* Location of bind source. */
  char* dst; /* Destination of bind. */
  char const* owner;
  char const* group;
  mode_t mode;
  int submount; /* Submount is bound already. */
} bind_mounts_default[] = {
    {str_literal(ENCRYPTED_MNT "/var"), str_literal("var"), "root", "root",
     S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, 0},
    {str_literal(ENCRYPTED_MNT "/chronos"), str_literal("home/chronos"),
     "chronos", "chronos", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, 1},
    {},
};
#undef str_literal

class EncryptedFs {
 public:
  result_code setup_encrypted(const char* encryption_key, int rebuild);
  result_code teardown_mount(void);
  result_code check_mount_states(void);
  result_code report_mount_info(void) const;
  result_code prepare_paths(gchar* mount_root);
  char* get_mount_key() const;

 private:
  // Paths used by the encrypted fs
  gchar* rootdir;
  gchar* stateful_mount;
  gchar* key_path;
  gchar* block_path;
  gchar* encrypted_mount;
  gchar* dmcrypt_name;
  gchar* dmcrypt_dev;
  bind_mount* bind_mounts;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOUNT_ENCRYPTED_ENCRYPTED_FS_H_
