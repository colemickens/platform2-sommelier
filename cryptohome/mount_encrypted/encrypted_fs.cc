// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mount_encrypted/encrypted_fs.h"

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <vboot/tlcl.h>

#include <string>

#include <brillo/secure_blob.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/mount_encrypted.h"
#include "cryptohome/mount_helpers.h"

namespace cryptohome {

namespace {

static const gchar* const kEncryptedFSType = "ext4";
static const gchar* const kCryptDevName = "encstateful";
static const float kSizePercent = 0.3;
static const uint64_t kSectorSize = 512;
static const uint64_t kExt4BlockSize = 4096;
static const uint64_t kExt4MinBytes = 16 * 1024 * 1024;
static const int kCryptAllowDiscard = 1;

result_code check_bind(struct bind_mount* bind, enum bind_dir dir) {
  struct passwd* user;
  struct group* group;
  const gchar* target;

  if (dir == BIND_SOURCE)
    target = bind->src;
  else
    target = bind->dst;

  if (access(target, R_OK) && mkdir(target, bind->mode)) {
    PERROR("mkdir(%s)", target);
    return RESULT_FAIL_FATAL;
  }

  /* Destination may be on read-only filesystem, so skip tweaks. */
  if (dir == BIND_DEST)
    return RESULT_SUCCESS;

  if (!(user = getpwnam(bind->owner))) {  // NOLINT(runtime/threadsafe_fn)
    PERROR("getpwnam(%s)", bind->owner);
    return RESULT_FAIL_FATAL;
  }
  if (!(group = getgrnam(bind->group))) {  // NOLINT(runtime/threadsafe_fn)
    PERROR("getgrnam(%s)", bind->group);
    return RESULT_FAIL_FATAL;
  }

  /* Must do explicit chmod since mkdir()'s mode respects umask. */
  if (chmod(target, bind->mode)) {
    PERROR("chmod(%s)", target);
    return RESULT_FAIL_FATAL;
  }
  if (chown(target, user->pw_uid, group->gr_gid)) {
    PERROR("chown(%s)", target);
    return RESULT_FAIL_FATAL;
  }

  return RESULT_SUCCESS;
}

void spawn_resizer(const char* device, uint64_t blocks, uint64_t blocks_max) {
  pid_t pid;

  /* Skip resize before forking, if it's not going to happen. */
  if (blocks >= blocks_max) {
    INFO("Resizing skipped. blocks:%" PRIu64 " >= blocks_max:%" PRIu64, blocks,
         blocks_max);
    return;
  }

  fflush(NULL);
  pid = fork();
  if (pid < 0) {
    PERROR("fork");
    return;
  }
  if (pid != 0) {
    INFO("Started filesystem resizing process %d.", pid);
    return;
  }

  /* Child */
  // TODO(sarthakkukreti): remove on refactor to ProcessImpl
  // along with vboot/tlcl.h.
  TlclLibClose();
  INFO_INIT("Resizer spawned.");

  if (daemon(0, 1)) {
    PERROR("daemon");
    goto out;
  }

  filesystem_resize(device, blocks, blocks_max);

out:
  INFO_DONE("Done.");
  exit(RESULT_SUCCESS);
}

/* This expects "mnt" to be allocated and initialized to NULL bytes. */
static result_code dup_bind_mount(struct bind_mount* mnt,
                                  struct bind_mount* old,
                                  char* dir) {
  if (old->src && asprintf(&mnt->src, "%s%s", dir, old->src) == -1)
    goto fail;
  if (old->dst && asprintf(&mnt->dst, "%s%s", dir, old->dst) == -1)
    goto fail;
  if (!(mnt->owner = strdup(old->owner)))
    goto fail;
  if (!(mnt->group = strdup(old->group)))
    goto fail;
  mnt->mode = old->mode;
  mnt->submount = old->submount;

  return RESULT_SUCCESS;

fail:
  perror(__FUNCTION__);
  return RESULT_FAIL_FATAL;
}

}  // namespace

/* Do all the work needed to actually set up the encrypted partition. */
result_code EncryptedFs::setup_encrypted(const char* encryption_key,
                                         int rebuild) {
  gchar* lodev = NULL;
  gchar* dirty_expire_centisecs = NULL;
  char* mount_opts = NULL;
  uint64_t commit_interval = 600;
  uint64_t sectors;
  struct bind_mount* bind;
  int sparsefd;
  struct statvfs stateful_statbuf;
  uint64_t blocks_min, blocks_max;
  result_code rc = RESULT_FAIL_FATAL;

  if (rebuild) {
    uint64_t fs_bytes_max;

    /* Wipe out the old files, and ignore errors. */
    unlink(block_path);

    /* Calculate the desired size of the new partition. */
    if (statvfs(stateful_mount, &stateful_statbuf)) {
      PERROR("%s", stateful_mount);
      goto finished;
    }
    fs_bytes_max = stateful_statbuf.f_blocks;
    fs_bytes_max *= kSizePercent;
    fs_bytes_max *= stateful_statbuf.f_frsize;

    INFO("Creating sparse backing file with size %" PRIu64 ".", fs_bytes_max);

    /* Create the sparse file. */
    sparsefd = sparse_create(block_path, fs_bytes_max);
    if (sparsefd < 0) {
      PERROR("%s", block_path);
      goto finished;
    }
  } else {
    sparsefd = open(block_path, O_RDWR | O_NOFOLLOW);
    if (sparsefd < 0) {
      PERROR("%s", block_path);
      goto finished;
    }
  }

  /* Set up loopback device. */
  INFO("Loopback attaching %s (named %s).", block_path, dmcrypt_name);
  lodev = loop_attach(sparsefd, dmcrypt_name);
  if (!lodev || strlen(lodev) == 0) {
    ERROR("loop_attach failed");
    goto finished;
  }

  /* Get size as seen by block device. */
  sectors = blk_size(lodev) / kSectorSize;
  if (!sectors) {
    ERROR("Failed to read device size");
    goto lo_cleanup;
  }

  /* Mount loopback device with dm-crypt using the encryption key. */
  INFO("Setting up dm-crypt %s as %s.", lodev, dmcrypt_dev);
  if (!dm_setup(sectors, encryption_key, dmcrypt_name, lodev, dmcrypt_dev,
                kCryptAllowDiscard)) {
    /* If dm_setup() fails, it could be due to lacking
     * "allow_discard" support, so try again with discard
     * disabled. There doesn't seem to be a way to query
     * the kernel for this feature short of a fallible
     * version test or just trying to set up the dm table
     * again, so do the latter.
     */
    if (!dm_setup(sectors, encryption_key, dmcrypt_name, lodev, dmcrypt_dev,
                  !kCryptAllowDiscard)) {
      ERROR("dm_setup failed");
      goto lo_cleanup;
    }
    INFO("%s: dm-crypt does not support discard; disabling.", dmcrypt_dev);
  }

  /* Calculate filesystem min/max size. */
  blocks_max = sectors / (kExt4BlockSize / kSectorSize);
  blocks_min = kExt4MinBytes / kExt4BlockSize;

  if (rebuild) {
    INFO(
        "Building filesystem on %s "
        "(blocksize:%" PRIu64 ", min:%" PRIu64 ", max:%" PRIu64 ").",
        dmcrypt_dev, kExt4BlockSize, blocks_min, blocks_max);
    if (!filesystem_build(dmcrypt_dev, kExt4BlockSize, blocks_min, blocks_max))
      goto dm_cleanup;
  }

  /* Use vm.dirty_expire_centisecs / 100 as the commit interval. */
  if (g_file_get_contents("/proc/sys/vm/dirty_expire_centisecs",
                          &dirty_expire_centisecs, NULL, NULL)) {
    guint64 dirty_expire = g_ascii_strtoull(dirty_expire_centisecs, NULL, 10);
    if (dirty_expire > 0)
      commit_interval = dirty_expire / 100;
    g_free(dirty_expire_centisecs);
  }
  if (asprintf(&mount_opts, "discard,commit=%" PRIu64, commit_interval) == -1)
    goto dm_cleanup;

  /* Mount the dm-crypt partition finally. */
  INFO("Mounting %s onto %s.", dmcrypt_dev, encrypted_mount);
  if (access(encrypted_mount, R_OK) &&
      mkdir(encrypted_mount, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
    PERROR("%s", dmcrypt_dev);
    goto dm_cleanup;
  }
  if (mount(dmcrypt_dev, encrypted_mount, kEncryptedFSType,
            MS_NODEV | MS_NOEXEC | MS_NOSUID | MS_NOATIME, mount_opts)) {
    PERROR("mount(%s,%s)", dmcrypt_dev, encrypted_mount);
    goto dm_cleanup;
  }

  /* Always spawn filesystem resizer, in case growth was interrupted. */
  /* TODO(keescook): if already full size, don't resize. */
  spawn_resizer(dmcrypt_dev, blocks_min, blocks_max);

  /* Perform bind mounts. */
  for (bind = bind_mounts; bind->src; ++bind) {
    INFO("Bind mounting %s onto %s.", bind->src, bind->dst);
    if (check_bind(bind, BIND_SOURCE) != RESULT_SUCCESS ||
        check_bind(bind, BIND_DEST) != RESULT_SUCCESS)
      goto unbind;
    if (mount(bind->src, bind->dst, "none", MS_BIND, NULL)) {
      PERROR("mount(%s,%s)", bind->src, bind->dst);
      goto unbind;
    }
  }

  /* Everything completed without error.*/
  rc = RESULT_SUCCESS;
  goto finished;

unbind:
  for (bind = bind_mounts; bind->src; ++bind) {
    INFO("Unmounting %s.", bind->dst);
    umount(bind->dst);
  }

  INFO("Unmounting %s.", encrypted_mount);
  umount(encrypted_mount);

dm_cleanup:
  INFO("Removing %s.", dmcrypt_dev);
  /* TODO(keescook): something holds this open briefly on mkfs failure
   * and I haven't been able to catch it yet. Adding an "fuser" call
   * here is sufficient to lose the race. Instead, just sleep during
   * the error path.
   */
  sleep(1);
  dm_teardown(dmcrypt_dev);

lo_cleanup:
  INFO("Unlooping %s.", lodev);
  loop_detach(lodev);

finished:
  free(lodev);
  free(mount_opts);

  return rc;
}

/* Clean up all bind mounts, mounts, attaches, etc. Only the final
 * action informs the return value. This makes it so that failures
 * can be cleaned up from, and continue the shutdown process on a
 * second call. If the loopback cannot be found, claim success.
 */
result_code EncryptedFs::teardown_mount(void) {
  struct bind_mount* bind;

  for (bind = bind_mounts; bind->src; ++bind) {
    INFO("Unmounting %s.", bind->dst);
    errno = 0;
    /* Allow either success or a "not mounted" failure. */
    if (umount(bind->dst)) {
      if (errno != EINVAL) {
        PERROR("umount(%s)", bind->dst);
        return RESULT_FAIL_FATAL;
      }
    }
  }

  INFO("Unmounting %s.", encrypted_mount);
  errno = 0;
  /* Allow either success or a "not mounted" failure. */
  if (umount(encrypted_mount)) {
    if (errno != EINVAL) {
      PERROR("umount(%s)", encrypted_mount);
      return RESULT_FAIL_FATAL;
    }
  }

  /*
   * Force syncs to make sure we don't tickle racey/buggy kernel
   * routines that might be causing crosbug.com/p/17610.
   */
  sync();

  /* Optionally run fsck on the device after umount. */
  if (getenv("MOUNT_ENCRYPTED_FSCK")) {
    char* cmd;

    if (asprintf(&cmd, "fsck -a %s", dmcrypt_dev) == -1) {
      PERROR("asprintf");
    } else {
      int rc;

      rc = system(cmd);
      if (rc != 0)
        ERROR("'%s' failed: %d", cmd, rc);
    }
  }

  INFO("Removing %s.", dmcrypt_dev);
  if (!dm_teardown(dmcrypt_dev))
    ERROR("dm_teardown(%s)", dmcrypt_dev);
  sync();

  INFO("Unlooping %s (named %s).", block_path, dmcrypt_name);
  if (!loop_detach_name(dmcrypt_name)) {
    ERROR("loop_detach_name(%s)", dmcrypt_name);
    return RESULT_FAIL_FATAL;
  }
  sync();

  return RESULT_SUCCESS;
}

result_code EncryptedFs::check_mount_states(void) {
  struct bind_mount* bind;

  /* Verify stateful partition exists. */
  if (access(stateful_mount, R_OK)) {
    INFO("%s does not exist.", stateful_mount);
    return RESULT_FAIL_FATAL;
  }
  /* Verify stateful is either a separate mount, or that the
   * root directory is writable (i.e. a factory install, dev mode
   * where root remounted rw, etc).
   */
  if (same_vfs(stateful_mount, rootdir) && access(rootdir, W_OK)) {
    INFO("%s is not mounted.", stateful_mount);
    return RESULT_FAIL_FATAL;
  }

  /* Verify encrypted partition is missing or not already mounted. */
  if (access(encrypted_mount, R_OK) == 0 &&
      !same_vfs(encrypted_mount, stateful_mount)) {
    INFO("%s already appears to be mounted.", encrypted_mount);
    return RESULT_SUCCESS;
  }

  /* Verify that bind mount targets exist. */
  for (bind = bind_mounts; bind->src; ++bind) {
    if (access(bind->dst, R_OK)) {
      PERROR("%s mount point is missing.", bind->dst);
      return RESULT_FAIL_FATAL;
    }
  }

  /* Verify that old bind mounts on stateful haven't happened yet. */
  for (bind = bind_mounts; bind->src; ++bind) {
    if (bind->submount)
      continue;

    if (same_vfs(bind->dst, stateful_mount)) {
      INFO("%s already bind mounted.", bind->dst);
      return RESULT_FAIL_FATAL;
    }
  }

  INFO("VFS mount state sanity check ok.");
  return RESULT_SUCCESS;
}

result_code EncryptedFs::report_mount_info(void) const {
  struct bind_mount* mnt;
  printf("rootdir: %s\n", rootdir);
  printf("stateful_mount: %s\n", stateful_mount);
  printf("key_path: %s\n", key_path);
  printf("block_path: %s\n", block_path);
  printf("encrypted_mount: %s\n", encrypted_mount);
  printf("dmcrypt_name: %s\n", dmcrypt_name);
  printf("dmcrypt_dev: %s\n", dmcrypt_dev);
  printf("bind mounts:\n");
  for (mnt = bind_mounts; mnt->src; ++mnt) {
    printf("\tsrc:%s\n", mnt->src);
    printf("\tdst:%s\n", mnt->dst);
    printf("\towner:%s\n", mnt->owner);
    printf("\tmode:%o\n", mnt->mode);
    printf("\tsubmount:%d\n", mnt->submount);
    printf("\n");
  }
  return RESULT_SUCCESS;
}

result_code EncryptedFs::prepare_paths(gchar* mount_root) {
  char* dir = NULL;
  struct bind_mount* old;
  struct bind_mount* mnt;

  mnt = bind_mounts = (struct bind_mount*)calloc(
      sizeof(bind_mounts_default) / sizeof(*bind_mounts_default),
      sizeof(*bind_mounts_default));
  if (!mnt) {
    perror("calloc");
    return RESULT_FAIL_FATAL;
  }

  if (mount_root != NULL) {
    brillo::SecureBlob digest;
    std::string hex;

    if (asprintf(&rootdir, "%s/", dir) == -1)
      goto fail;

    /* Generate a shortened hash for non-default cryptnames,
     * which will get re-used in the loopback name, which
     * must be less than 64 (LO_NAME_SIZE) bytes. */
    digest = CryptoLib::Sha256(brillo::SecureBlob(std::string(mount_root)));
    hex = CryptoLib::BlobToHex(digest).substr(0, 16);
    if (asprintf(&dmcrypt_name, "%s_%s", kCryptDevName, hex.c_str()) == -1)
      goto fail;
  } else {
    rootdir = const_cast<gchar*>("/");
    if (!(dmcrypt_name = strdup(kCryptDevName)))
      goto fail;
  }

  if (asprintf(&stateful_mount, "%s%s", rootdir, STATEFUL_MNT) == -1)
    goto fail;
  if (asprintf(&key_path, "%s%s", rootdir, STATEFUL_MNT "/encrypted.key") == -1)
    goto fail;
  if (asprintf(&block_path, "%s%s", rootdir, STATEFUL_MNT "/encrypted.block") ==
      -1)
    goto fail;
  if (asprintf(&encrypted_mount, "%s%s", rootdir, ENCRYPTED_MNT) == -1)
    goto fail;
  if (asprintf(&dmcrypt_dev, "/dev/mapper/%s", dmcrypt_name) == -1)
    goto fail;

  for (old = bind_mounts_default; old->src; ++old) {
    result_code rc = dup_bind_mount(mnt++, old, rootdir);
    if (rc != RESULT_SUCCESS)
      return rc;
  }

  return RESULT_SUCCESS;

fail:
  perror("asprintf");
  return RESULT_FAIL_FATAL;
}

char* EncryptedFs::get_mount_key() const {
  return dm_get_key(dmcrypt_dev);
}

}  // namespace cryptohome
