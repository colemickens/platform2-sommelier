/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This tool will attempt to mount or create the encrypted stateful partition,
 * and the various bind mountable subdirectories.
 *
 */
#define _FILE_OFFSET_BITS 64
#define CHROMEOS_ENVIRONMENT

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <grp.h>
#include <inttypes.h>
#include <openssl/rand.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <vboot/crossystem.h>
#include <vboot/tlcl.h>

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>

#include <metrics/metrics_library.h>

#include <cryptohome/cryptolib.h>
#include "cryptohome/mount_encrypted.h"
#include "cryptohome/mount_encrypted/encryption_key.h"
#include "cryptohome/mount_encrypted/tpm.h"
#include "cryptohome/mount_helpers.h"

#define STATEFUL_MNT "mnt/stateful_partition"
#define ENCRYPTED_MNT STATEFUL_MNT "/encrypted"
#define BUF_SIZE 1024
#define PROP_SIZE 64

constexpr char kBioCryptoInitPath[] = "/usr/bin/bio_crypto_init";
constexpr char kBioTpmSeedSalt[] = "biod";
constexpr char kBioTpmSeedTmpDir[] = "/run/bio_crypto_init";
constexpr char kBioTpmSeedFile[] = "seed";
static const gchar* const kEncryptedFSType = "ext4";
static const gchar* const kCryptDevName = "encstateful";
static const gchar* const kNvramExport = "/tmp/lockbox.nvram";
static const float kSizePercent = 0.3;
static const float kMigrationSizeMultiplier = 1.1;
static const uint64_t kSectorSize = 512;
static const uint64_t kExt4BlockSize = 4096;
static const uint64_t kExt4MinBytes = 16 * 1024 * 1024;
static const int kCryptAllowDiscard = 1;
static const uid_t kBiodUid = 282;
static const gid_t kBiodGid = 282;

enum migration_method {
  MIGRATE_TEST_ONLY,
  MIGRATE_FOR_REAL,
};

enum bind_dir {
  BIND_SOURCE,
  BIND_DEST,
};

#define str_literal(s) (const_cast<char *>(s))
static struct bind_mount {
  char* src;      /* Location of bind source. */
  char* dst;      /* Destination of bind. */
  char* previous; /* Migratable prior bind source. */
  char* pending;  /* Location for pending deletion. */
  char const* owner;
  char const* group;
  mode_t mode;
  int submount; /* Submount is bound already. */
} bind_mounts_default[] = {
    {str_literal(ENCRYPTED_MNT "/var"), str_literal("var"),
     str_literal(STATEFUL_MNT "/var"), str_literal(STATEFUL_MNT "/.var"),
     "root", "root",
     S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, 0},
    {str_literal(ENCRYPTED_MNT "/chronos"), str_literal("home/chronos"),
     str_literal(STATEFUL_MNT "/home/chronos"),
     str_literal(STATEFUL_MNT "/home/.chronos"),
     "chronos", "chronos",
     S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, 1},
    {},
};
#undef str_literal

#if DEBUG_ENABLED
struct timeval tick = {};
struct timeval tick_start = {};
#endif

static struct bind_mount* bind_mounts = NULL;

static gchar* rootdir = NULL;
static gchar* stateful_mount = NULL;
static gchar* key_path = NULL;
static gchar* block_path = NULL;
static gchar* encrypted_mount = NULL;
static gchar* dmcrypt_name = NULL;
static gchar* dmcrypt_dev = NULL;

static const char kMountEncryptedMetricsPath[] = "/run/metrics.mount-encrypted";

namespace metrics {
const char kSystemKeyStatus[] = "Platform.MountEncrypted.SystemKeyStatus";
const char kEncryptionKeyStatus[] =
    "Platform.MountEncrypted.EncryptionKeyStatus";
}

static void sha256(char const* str, uint8_t* digest) {
  SHA256((unsigned char*)(str), strlen(str), digest);
}

static result_code get_system_property(const char* prop, char* buf,
                                       size_t length) {
  const char* rc;

  DEBUG("Fetching System Property '%s'", prop);
  rc = VbGetSystemPropertyString(prop, buf, length);
  DEBUG("Got System Property 'mainfw_type': %s", rc ? buf : "FAIL");

  return rc != NULL ? RESULT_SUCCESS : RESULT_FAIL_FATAL;
}

static int has_chromefw(void) {
  static int state = -1;
  char fw[PROP_SIZE];

  /* Cache the state so we don't have to perform the query again. */
  if (state != -1)
    return state;

  if (get_system_property("mainfw_type", fw, sizeof(fw)) != RESULT_SUCCESS)
    state = 0;
  else
    state = strcmp(fw, "nonchrome") != 0;
  return state;
}

static result_code check_bind(struct bind_mount* bind, enum bind_dir dir) {
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

static result_code migrate_contents(struct bind_mount* bind,
                            enum migration_method method) {
  const gchar* previous = NULL;
  const gchar* pending = NULL;
  gchar* dotdir;

  /* Skip migration if the previous bind sources are missing. */
  if (bind->pending && access(bind->pending, R_OK) == 0)
    pending = bind->pending;
  if (bind->previous && access(bind->previous, R_OK) == 0)
    previous = bind->previous;
  if (!pending && !previous)
    return RESULT_FAIL_FATAL;

  /* Pretend migration happened. */
  if (method == MIGRATE_TEST_ONLY)
    return RESULT_SUCCESS;

  check_bind(bind, BIND_SOURCE);

  /* Prefer the pending-delete location when doing migration. */
  if (!(dotdir = g_strdup_printf("%s/.", pending ? pending : previous))) {
    PERROR("g_strdup_printf");
    goto mark_for_removal;
  }

  INFO("Migrating bind mount contents %s to %s.", dotdir, bind->src);

  /* Add scope to ensure compilation with C++: "error: jump bypasses
   * initialization" goto mark_for_removal jumps over the definition of cp[] */
  {
    const gchar* cp[] = {"/bin/cp", "-a", dotdir, bind->src, NULL};

    if (runcmd(cp, NULL) != 0) {
      /* If the copy failed, it may have partially populated the
       * new source, so we need to remove the new source and
       * rebuild it. Regardless, the previous source must be removed
       * as well.
       */
      INFO("Failed to migrate %s to %s!", dotdir, bind->src);
      remove_tree(bind->src);
      check_bind(bind, BIND_SOURCE);
    }
  }

mark_for_removal:
  g_free(dotdir);

  /* The removal of the previous directory needs to happen at finalize
   * time, otherwise /var state gets lost on a migration if the
   * system is powered off before the encryption key is saved. Instead,
   * relocate the directory so it can be removed (or re-migrated).
   */

  if (previous) {
    /* If both pending and previous directory exists, we must
     * remove previous entirely now so it stops taking up disk
     * space. The pending area will stay pending to be deleted
     * later.
     */
    if (pending)
      remove_tree(pending);
    if (rename(previous, bind->pending)) {
      PERROR("rename(%s,%s)", previous, bind->pending);
    }
  }

  /* As noted above, failures are unrecoverable, so getting here means
   * "we're done" more than "it worked".
   */
  return RESULT_SUCCESS;
}

void remove_pending() {
  struct bind_mount* bind;

  for (bind = bind_mounts; bind->src; ++bind) {
    if (!bind->pending || access(bind->pending, R_OK))
      continue;
    INFO("Removing %s.", bind->pending);
#if DEBUG_ENABLED
    continue;
#endif
    remove_tree(bind->pending);
  }
}

// This triggers the live encryption key to be written to disk, encrypted by the
// system key. It is intended to be called by Cryptohome once the TPM is done
// being set up. If the system key is passed as an argument, use it, otherwise
// attempt to query the TPM again.
static result_code finalize_from_cmdline(char* key) {
  // Load the system key.
  brillo::SecureBlob system_key;
  if (!base::HexStringToBytes(key, &system_key) ||
      system_key.size() != DIGEST_LENGTH) {
    LOG(ERROR) << "Failed to parse system key.";
    return RESULT_FAIL_FATAL;
  }

  FixedSystemKeyLoader loader(system_key);
  EncryptionKey key_manager(&loader, base::FilePath(rootdir));
  result_code rc = key_manager.SetTpmSystemKey();
  if (rc != RESULT_SUCCESS) {
    return rc;
  }

  // If there already is an encrypted system key on disk, there is nothing to
  // do. This also covers cases where the system key is not derived from the
  // lockbox space contents (e.g. TPM 2.0 devices, TPM 1.2 devices with
  // encrypted stateful space, factory keys, etc.), for which it is not
  // appropriate to replace the system key. For cases where finalization is
  // unfinished, we clear any stale system keys from disk to make sure we pass
  // the check here.
  if (base::PathExists(key_manager.key_path())) {
    return RESULT_SUCCESS;
  }

  // Load the encryption key.
  char* encryption_key = dm_get_key(dmcrypt_dev);
  if (!encryption_key) {
    ERROR("Could not locate encryption key for %s.", dmcrypt_dev);
    return RESULT_FAIL_FATAL;
  }
  brillo::SecureBlob encryption_key_blob;
  if (!base::HexStringToBytes(encryption_key, &encryption_key_blob)) {
    ERROR("Failed to decode encryption key.");
    return RESULT_FAIL_FATAL;
  }

  // Persist the encryption key to disk.
  key_manager.PersistEncryptionKey(encryption_key_blob);

  return RESULT_SUCCESS;
}

static void spawn_resizer(const char* device,
                          uint64_t blocks,
                          uint64_t blocks_max) {
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

/* Do all the work needed to actually set up the encrypted partition. */
static result_code setup_encrypted(const char* encryption_key,
                                   int rebuild,
                                   int migrate_allowed) {
  brillo::SecureBlob system_key;
  int migrate_needed = 0;
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

  /* Decide now if any migration will happen. If so, we will not
   * grow the new filesystem in the background, since we need to
   * copy the contents over before /var is valid again.
   */
  if (!rebuild)
    migrate_allowed = 0;
  if (migrate_allowed) {
    for (bind = bind_mounts; bind->src; ++bind) {
      if (migrate_contents(bind, MIGRATE_TEST_ONLY) == RESULT_SUCCESS)
        migrate_needed = 1;
    }
  }

  /* Calculate filesystem min/max size. */
  blocks_max = sectors / (kExt4BlockSize / kSectorSize);
  blocks_min = kExt4MinBytes / kExt4BlockSize;
  if (migrate_needed && migrate_allowed) {
    uint64_t fs_bytes_min;
    uint64_t calc_blocks_min;
    /* When doing a migration, the new filesystem must be
     * large enough to hold what we're going to migrate.
     * Instead of walking the bind mount sources, which would
     * be IO and time expensive, just read the bytes-used
     * value from statvfs (plus 10% for overhead). It will
     * be too large, since it includes the eCryptFS data, so
     * we must cap at the max filesystem size just in case.
     */

    /* Bytes used in stateful partition plus 10%. */
    fs_bytes_min = stateful_statbuf.f_blocks - stateful_statbuf.f_bfree;
    fs_bytes_min *= stateful_statbuf.f_frsize;
    DEBUG("Stateful bytes used: %" PRIu64 "", fs_bytes_min);
    fs_bytes_min *= kMigrationSizeMultiplier;

    /* Minimum blocks needed for that many bytes. */
    calc_blocks_min = fs_bytes_min / kExt4BlockSize;
    /* Do not use more than blocks_max. */
    if (calc_blocks_min > blocks_max)
      calc_blocks_min = blocks_max;
    /* Do not use less than blocks_min. */
    else if (calc_blocks_min < blocks_min)
      calc_blocks_min = blocks_min;

    DEBUG("Maximum fs blocks: %" PRIu64 "", blocks_max);
    DEBUG("Minimum fs blocks: %" PRIu64 "", blocks_min);
    DEBUG("Migration blocks chosen: %" PRIu64 "", calc_blocks_min);
    blocks_min = calc_blocks_min;
  }

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

  /* If the legacy lockbox NVRAM area exists, we've rebuilt the
   * filesystem, and there are old bind sources on disk, attempt
   * migration.
   */
  if (migrate_needed && migrate_allowed) {
    /* Migration needs to happen before bind mounting because
     * some partitions were not already on the stateful partition,
     * and would be over-mounted by the new bind mount.
     */
    for (bind = bind_mounts; bind->src; ++bind)
      migrate_contents(bind, MIGRATE_FOR_REAL);
  }

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
static result_code shutdown(void) {
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

static result_code check_mount_states(void) {
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

static result_code report_info() {
  struct bind_mount* mnt;

  Tpm tpm;
  printf("TPM: %s\n", tpm.available() ? "yes" : "no");
  if (tpm.available()) {
    bool owned = false;
    printf("TPM Owned: %s\n", tpm.IsOwned(&owned) == RESULT_SUCCESS
                                  ? (owned ? "yes" : "no")
                                  : "fail");
  }
  printf("ChromeOS: %s\n", has_chromefw() ? "yes" : "no");
  printf("TPM2: %s\n", tpm.is_tpm2() ? "yes" : "no");
  if (has_chromefw()) {
    brillo::SecureBlob system_key;
    auto loader = SystemKeyLoader::Create(&tpm, base::FilePath(rootdir));
    result_code rc = loader->Load(&system_key);
    if (rc != RESULT_SUCCESS) {
      printf("NVRAM: missing.\n");
    } else {
      printf("NVRAM: available.\n");
    }
  } else {
    printf("NVRAM: not present\n");
  }

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
    printf("\tprevious:%s\n", mnt->previous);
    printf("\tpending:%s\n", mnt->pending);
    printf("\towner:%s\n", mnt->owner);
    printf("\tmode:%o\n", mnt->mode);
    printf("\tsubmount:%d\n", mnt->submount);
    printf("\n");
  }

  return RESULT_SUCCESS;
}

/* This expects "mnt" to be allocated and initialized to NULL bytes. */
static result_code dup_bind_mount(struct bind_mount* mnt,
                          struct bind_mount* old,
                          char* dir) {
  if (old->src && asprintf(&mnt->src, "%s%s", dir, old->src) == -1)
    goto fail;
  if (old->dst && asprintf(&mnt->dst, "%s%s", dir, old->dst) == -1)
    goto fail;
  if (old->previous &&
      asprintf(&mnt->previous, "%s%s", dir, old->previous) == -1)
    goto fail;
  if (old->pending && asprintf(&mnt->pending, "%s%s", dir, old->pending) == -1)
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

static result_code prepare_paths(void) {
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

  if ((dir = getenv("MOUNT_ENCRYPTED_ROOT")) != NULL) {
    unsigned char digest[DIGEST_LENGTH];
    gchar* hex;

    if (asprintf(&rootdir, "%s/", dir) == -1)
      goto fail;

    /* Generate a shortened hash for non-default cryptnames,
     * which will get re-used in the loopback name, which
     * must be less than 64 (LO_NAME_SIZE) bytes. */
    sha256(dir, digest);
    hex = stringify_hex(digest, sizeof(digest));
    hex[17] = '\0';
    if (asprintf(&dmcrypt_name, "%s_%s", kCryptDevName, hex) == -1)
      goto fail;
    g_free(hex);
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

/* Exports NVRAM contents to tmpfs for use by install attributes */
void nvram_export(const brillo::SecureBlob& contents) {
  int fd;
  DEBUG("Export NVRAM contents");
  fd = open(kNvramExport, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    perror("open(nvram_export)");
    return;
  }
  if (write(fd, contents.data(), contents.size()) != contents.size()) {
    /* Don't leave broken files around */
    unlink(kNvramExport);
  }
  close(fd);
}

template <typename Enum>
void RecordEnumeratedHistogram(MetricsLibrary* metrics,
                               const char* name,
                               Enum val) {
  metrics->SendEnumToUMA(name, static_cast<int>(val),
                         static_cast<int>(Enum::kCount));
}

/*
 * Send a secret derived from the system key to the biometric managers, if
 * available, via a tmpfs file which will be read by bio_crypto_init.
 */
bool SendSecretToBiodTmpFile(const EncryptionKey& key) {
  /* If there isn't a bio-sensor, don't bother. */
  if (!base::PathExists(base::FilePath(kBioCryptoInitPath))) {
    LOG(INFO) << "There is no biod, so skip sending TPM seed.";
    return true;
  }

  brillo::SecureBlob tpm_seed =
      key.GetDerivedSystemKey(std::string(kBioTpmSeedSalt));
  if (tpm_seed.empty()) {
    LOG(ERROR) << "TPM Seed provided is empty, not writing to tmpfs.";
    return false;
  }

  auto dirname = base::FilePath(kBioTpmSeedTmpDir);
  if (!base::CreateDirectory(dirname)) {
    LOG(ERROR) << "Failed to create dir for TPM seed.";
    return false;
  }

  if (chown(kBioTpmSeedTmpDir, kBiodUid, kBiodGid) == -1) {
    PLOG(ERROR) << "Failed to change ownership of biod tmpfs dir.";
    return false;
  }

  auto filename = dirname.Append(kBioTpmSeedFile);
  if (base::WriteFile(filename, tpm_seed.char_data(), tpm_seed.size()) !=
      tpm_seed.size()) {
    LOG(ERROR) << "Failed to write TPM seed to tmpfs file.";
    return false;
  }

  if (chown(filename.value().c_str(), kBiodUid, kBiodGid) == -1) {
    PLOG(ERROR) << "Failed to change ownership of biod tmpfs file.";
    return false;
  }

  return true;
}

int main(int argc, char* argv[]) {
  result_code rc;

  MetricsLibrary metrics;
  metrics.Init();
  metrics.SetOutputFile(kMountEncryptedMetricsPath);

  INFO_INIT("Starting.");
  rc = prepare_paths();
  if (rc != RESULT_SUCCESS)
    return rc;

  bool use_factory_system_key = false;
  if (argc > 1) {
    if (!strcmp(argv[1], "umount")) {
      return shutdown();
    } else if (!strcmp(argv[1], "info")) {
      return report_info();
    } else if (!strcmp(argv[1], "finalize")) {
      return finalize_from_cmdline(argc > 2 ? argv[2] : NULL);
    } else if (!strcmp(argv[1], "factory")) {
      use_factory_system_key = true;
    } else {
      fprintf(stderr, "Usage: %s [info|finalize|umount|factory]\n", argv[0]);
      return RESULT_FAIL_FATAL;
    }
  }

  /* For the mount operation at boot, return RESULT_FAIL_FATAL to trigger
   * chromeos_startup do the stateful wipe.
   */
  rc = check_mount_states();
  if (rc != RESULT_SUCCESS)
    return rc;

  Tpm tpm;
  auto loader = SystemKeyLoader::Create(&tpm, base::FilePath(rootdir));
  EncryptionKey key(loader.get(), base::FilePath(rootdir));
  if (use_factory_system_key) {
    rc = key.SetFactorySystemKey();
  } else if (has_chromefw()) {
    rc = key.LoadChromeOSSystemKey();
  } else {
    rc = key.SetInsecureFallbackSystemKey();
  }
  RecordEnumeratedHistogram(&metrics, metrics::kSystemKeyStatus,
                            key.system_key_status());
  if (rc != RESULT_SUCCESS) {
    return rc;
  }

  rc = key.LoadEncryptionKey();
  RecordEnumeratedHistogram(&metrics, metrics::kEncryptionKeyStatus,
                            key.encryption_key_status());
  if (rc != RESULT_SUCCESS) {
    return rc;
  }

  /* Log errors during sending seed to biod, but don't stop execution. */
  if (!use_factory_system_key && has_chromefw()) {
    if (!SendSecretToBiodTmpFile(key)) {
      LOG(ERROR) << "Failed to send TPM secret to biod.";
    }
  } else {
    LOG(ERROR) << "Failed to load system key, biod won't get a TPM seed.";
  }

  std::string encryption_key_hex =
      base::HexEncode(key.encryption_key().data(), key.encryption_key().size());
  rc = setup_encrypted(encryption_key_hex.c_str(), key.is_fresh(), false);
  if (rc == RESULT_SUCCESS) {
    if (key.did_finalize()) {
      remove_pending();
    }

    bool lockbox_valid = false;
    if (loader->CheckLockbox(&lockbox_valid) == RESULT_SUCCESS) {
      NvramSpace* lockbox_space = tpm.GetLockboxSpace();
      if (lockbox_valid && lockbox_space->is_valid()) {
        LOG(INFO) << "Lockbox is valid, exporting.";
        nvram_export(lockbox_space->contents());
      }
    } else {
      LOG(ERROR) << "Lockbox validity check error.";
    }
  }

  INFO_DONE("Done.");

  /* Continue boot. */
  return rc;
}
