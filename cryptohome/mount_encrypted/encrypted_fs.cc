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

#include <base/strings/string_number_conversions.h>
#include <brillo/secure_blob.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/mount_encrypted.h"
#include "cryptohome/mount_helpers.h"

namespace cryptohome {

namespace {

constexpr char kEncryptedFSType[] = "ext4";
constexpr char kCryptDevName[] = "encstateful";
constexpr char kDevMapperPath[] = "/dev/mapper";
constexpr float kSizePercent = 0.3;
constexpr uint64_t kSectorSize = 512;
constexpr uint64_t kExt4BlockSize = 4096;
constexpr uint64_t kExt4MinBytes = 16 * 1024 * 1024;
constexpr int kCryptAllowDiscard = 1;

// Temp. function to cleanly get the c_str() of FilePaths.
// TODO(sarthakkukreti): Remove once all functions use base::FilePath
const char* path_str(const base::FilePath& path) {
  return path.value().c_str();
}

result_code CheckBind(BindMount* bind, BindDir dir) {
  struct passwd* user;
  struct group* group;
  base::FilePath target;

  if (dir == BindDir::BIND_SOURCE)
    target = bind->src;
  else
    target = bind->dst;

  if (access(path_str(target), R_OK) && mkdir(path_str(target), bind->mode)) {
    PLOG(ERROR) << "mkdir: " << target;
    return RESULT_FAIL_FATAL;
  }

  // Destination may be on read-only filesystem, so skip tweaks.
  if (dir == BindDir::BIND_DEST)
    return RESULT_SUCCESS;

  if (!(user =
            getpwnam(bind->owner.c_str()))) {  // NOLINT(runtime/threadsafe_fn)
    PLOG(ERROR) << "getpwnam: " << bind->owner;
    return RESULT_FAIL_FATAL;
  }
  if (!(group =
            getgrnam(bind->group.c_str()))) {  // NOLINT(runtime/threadsafe_fn)
    PLOG(ERROR) << "getgrnam: " << bind->group;
    return RESULT_FAIL_FATAL;
  }

  // Must do explicit chmod since mkdir()'s mode respects umask.
  if (chmod(path_str(target), bind->mode)) {
    PLOG(ERROR) << "chmod: " << target;
    return RESULT_FAIL_FATAL;
  }
  if (chown(path_str(target), user->pw_uid, group->gr_gid)) {
    PLOG(ERROR) << "chown: " << target;
    return RESULT_FAIL_FATAL;
  }

  return RESULT_SUCCESS;
}

void SpawnResizer(const base::FilePath& device,
                  uint64_t blocks,
                  uint64_t blocks_max) {
  pid_t pid;

  // Skip resize before forking, if it's not going to happen.
  if (blocks >= blocks_max) {
    LOG(INFO) << "Resizing skipped. blocks: " << blocks
              << " >= blocks_max: " << blocks_max;
    return;
  }

  fflush(NULL);
  pid = fork();
  if (pid < 0) {
    PERROR("fork");
    return;
  }
  if (pid != 0) {
    LOG(INFO) << "Started filesystem resizing process: " << pid;
    return;
  }

  // Child
  // TODO(sarthakkukreti): remove on refactor to ProcessImpl
  // along with vboot/tlcl.h.
  TlclLibClose();
  LOG(INFO) << "Resizer spawned.";

  if (daemon(0, 1)) {
    PERROR("daemon");
    goto out;
  }

  filesystem_resize(path_str(device), blocks, blocks_max);

out:
  LOG(INFO) << "Done.";
  exit(RESULT_SUCCESS);
}

}  // namespace

EncryptedFs::EncryptedFs(const base::FilePath& mount_root) {
  dmcrypt_name = std::string(kCryptDevName);
  rootdir = base::FilePath("/");
  if (!mount_root.empty()) {
    brillo::SecureBlob digest =
        cryptohome::CryptoLib::Sha256(brillo::SecureBlob(mount_root.value()));
    std::string hex = cryptohome::CryptoLib::BlobToHex(digest);
    dmcrypt_name += "_" + hex.substr(0, 16);
    rootdir = mount_root;
  }
  // Initialize remaining directories.
  stateful_mount = rootdir.Append(STATEFUL_MNT);
  block_path = rootdir.Append(STATEFUL_MNT "/encrypted.block");
  encrypted_mount = rootdir.Append(ENCRYPTED_MNT);
  dmcrypt_dev = base::FilePath(kDevMapperPath).Append(dmcrypt_name.c_str());

  // Create bind mounts.
  bind_mounts.push_back(
      {rootdir.Append(ENCRYPTED_MNT "/var"), rootdir.Append("var"), "root",
       "root", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, false});

  bind_mounts.push_back({rootdir.Append(ENCRYPTED_MNT "/chronos"),
                         rootdir.Append("home/chronos"), "chronos", "chronos",
                         S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH,
                         true});
}

// Do all the work needed to actually set up the encrypted partition.
result_code EncryptedFs::Setup(const brillo::SecureBlob& encryption_key,
                               bool rebuild) {
  gchar* lodev = NULL;
  gchar* dirty_expire_centisecs = NULL;
  char* mount_opts = NULL;
  uint64_t commit_interval = 600;
  uint64_t sectors;
  int sparsefd;
  struct statvfs stateful_statbuf;
  uint64_t blocks_min, blocks_max;
  result_code rc = RESULT_FAIL_FATAL;
  std::string encryption_key_hex;

  if (rebuild) {
    uint64_t fs_bytes_max;

    // Wipe out the old files, and ignore errors.
    unlink(path_str(block_path));

    // Calculate the desired size of the new partition.
    if (statvfs(path_str(stateful_mount), &stateful_statbuf)) {
      PLOG(ERROR) << stateful_mount;
      goto finished;
    }
    fs_bytes_max = stateful_statbuf.f_blocks;
    fs_bytes_max *= kSizePercent;
    fs_bytes_max *= stateful_statbuf.f_frsize;

    LOG(INFO) << "Creating sparse backing file with size " << fs_bytes_max;

    // Create the sparse file.
    sparsefd = sparse_create(path_str(block_path), fs_bytes_max);
    if (sparsefd < 0) {
      PLOG(ERROR) << block_path;
      goto finished;
    }
  } else {
    sparsefd = open(path_str(block_path), O_RDWR | O_NOFOLLOW);
    if (sparsefd < 0) {
      PLOG(ERROR) << block_path;
      goto finished;
    }
  }

  // Set up loopback device.
  LOG(INFO) << "Loopback attaching " << block_path << " named " << dmcrypt_name;
  lodev = loop_attach(sparsefd, dmcrypt_name.c_str());
  if (!lodev || strlen(lodev) == 0) {
    LOG(ERROR) << "loop_attach failed";
    goto finished;
  }

  // Get size as seen by block device.
  sectors = blk_size(lodev) / kSectorSize;
  if (!sectors) {
    LOG(ERROR) << "Failed to read device size";
    goto lo_cleanup;
  }

  // Mount loopback device with dm-crypt using the encryption key.
  LOG(INFO) << "Setting up dm-crypt " << lodev << " as " << dmcrypt_dev;

  encryption_key_hex =
      base::HexEncode(encryption_key.data(), encryption_key.size());
  if (!dm_setup(sectors, encryption_key_hex.c_str(), dmcrypt_name.c_str(),
                lodev, path_str(dmcrypt_dev), kCryptAllowDiscard)) {
    // If dm_setup() fails, it could be due to lacking
    // "allow_discard" support, so try again with discard
    // disabled. There doesn't seem to be a way to query
    // the kernel for this feature short of a fallible
    // version test or just trying to set up the dm table
    // again, so do the latter.
    //
    if (!dm_setup(sectors, encryption_key_hex.c_str(), dmcrypt_name.c_str(),
                  lodev, path_str(dmcrypt_dev), !kCryptAllowDiscard)) {
      LOG(ERROR) << "dm_setup failed";
      goto lo_cleanup;
    }
    LOG(INFO) << dmcrypt_dev
              << ": dm-crypt does not support discard; disabling.";
  }

  // Calculate filesystem min/max size.
  blocks_max = sectors / (kExt4BlockSize / kSectorSize);
  blocks_min = kExt4MinBytes / kExt4BlockSize;

  if (rebuild) {
    LOG(INFO) << "Building filesystem on " << dmcrypt_dev
              << "(blocksize: " << kExt4BlockSize << ", min: " << blocks_min
              << ", max: " << blocks_max;
    if (!filesystem_build(path_str(dmcrypt_dev), kExt4BlockSize, blocks_min,
                          blocks_max))
      goto dm_cleanup;
  }

  // Use vm.dirty_expire_centisecs / 100 as the commit interval.
  if (g_file_get_contents("/proc/sys/vm/dirty_expire_centisecs",
                          &dirty_expire_centisecs, NULL, NULL)) {
    guint64 dirty_expire = g_ascii_strtoull(dirty_expire_centisecs, NULL, 10);
    if (dirty_expire > 0)
      commit_interval = dirty_expire / 100;
    g_free(dirty_expire_centisecs);
  }
  if (asprintf(&mount_opts, "discard,commit=%" PRIu64, commit_interval) == -1)
    goto dm_cleanup;

  // Mount the dm-crypt partition finally.
  LOG(INFO) << "Mounting " << dmcrypt_dev << " onto " << encrypted_mount;
  if (access(path_str(encrypted_mount), R_OK) &&
      mkdir(path_str(encrypted_mount), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
    PLOG(ERROR) << dmcrypt_dev;
    goto dm_cleanup;
  }
  if (mount(path_str(dmcrypt_dev), path_str(encrypted_mount), kEncryptedFSType,
            MS_NODEV | MS_NOEXEC | MS_NOSUID | MS_NOATIME, mount_opts)) {
    PLOG(ERROR) << "mount: " << dmcrypt_dev << ", " << encrypted_mount;
    goto dm_cleanup;
  }

  // Always spawn filesystem resizer, in case growth was interrupted.
  // TODO(keescook): if already full size, don't resize.
  SpawnResizer(dmcrypt_dev, blocks_min, blocks_max);

  // Perform bind mounts.
  for (auto& bind : bind_mounts) {
    LOG(INFO) << "Bind mounting " << bind.src << " onto " << bind.dst;
    if (CheckBind(&bind, BindDir::BIND_SOURCE) != RESULT_SUCCESS ||
        CheckBind(&bind, BindDir::BIND_DEST) != RESULT_SUCCESS)
      goto unbind;
    if (mount(path_str(bind.src), path_str(bind.dst), "none", MS_BIND, NULL)) {
      PLOG(ERROR) << "mount: " << bind.src << ", " << bind.dst;
      goto unbind;
    }
  }

  // Everything completed without error.
  rc = RESULT_SUCCESS;
  goto finished;

unbind:
  for (auto& bind : bind_mounts) {
    LOG(INFO) << "Unmounting " << bind.dst;
    umount(path_str(bind.dst));
  }

  LOG(INFO) << "Unmounting " << encrypted_mount;
  umount(path_str(encrypted_mount));

dm_cleanup:
  LOG(INFO) << "Removing " << dmcrypt_dev;
  // TODO(keescook): something holds this open briefly on mkfs failure
  // and I haven't been able to catch it yet. Adding an "fuser" call
  // here is sufficient to lose the race. Instead, just sleep during
  // the error path.
  sleep(1);
  dm_teardown(path_str(dmcrypt_dev));

lo_cleanup:
  LOG(INFO) << "Unlooping " << lodev;
  loop_detach(lodev);

finished:
  free(lodev);
  free(mount_opts);

  return rc;
}

// Clean up all bind mounts, mounts, attaches, etc. Only the final
// action informs the return value. This makes it so that failures
// can be cleaned up from, and continue the shutdown process on a
// second call. If the loopback cannot be found, claim success.
result_code EncryptedFs::Teardown(void) {
  for (auto& bind : bind_mounts) {
    LOG(INFO) << "Unmounting " << bind.dst;
    errno = 0;
    // Allow either success or a "not mounted" failure.
    if (umount(path_str(bind.dst))) {
      if (errno != EINVAL) {
        PLOG(ERROR) << "umount " << bind.dst;
        return RESULT_FAIL_FATAL;
      }
    }
  }

  LOG(INFO) << "Unmounting " << encrypted_mount;
  errno = 0;
  // Allow either success or a "not mounted" failure.
  if (umount(path_str(encrypted_mount))) {
    if (errno != EINVAL) {
      PLOG(ERROR) << "umount " << encrypted_mount;
      return RESULT_FAIL_FATAL;
    }
  }

  // Force syncs to make sure we don't tickle racey/buggy kernel
  // routines that might be causing crosbug.com/p/17610.
  sync();

  // Optionally run fsck on the device after umount.
  if (getenv("MOUNT_ENCRYPTED_FSCK")) {
    char* cmd;

    if (asprintf(&cmd, "fsck -a %s", path_str(dmcrypt_dev)) == -1) {
      PLOG(ERROR) << "asprintf";
    } else {
      int rc;

      rc = system(cmd);
      if (rc != 0)
        PLOG(ERROR) << cmd << " failed: " << rc;
    }
  }

  LOG(INFO) << "Removing " << dmcrypt_dev;
  if (!dm_teardown(path_str(dmcrypt_dev)))
    LOG(ERROR) << "dm_teardown: " << dmcrypt_dev;
  sync();

  LOG(INFO) << "Unlooping " << block_path << " named " << dmcrypt_name;
  if (!loop_detach_name(dmcrypt_name.c_str())) {
    LOG(ERROR) << "loop_detach_name: " << dmcrypt_name;
    return RESULT_FAIL_FATAL;
  }
  sync();

  return RESULT_SUCCESS;
}

result_code EncryptedFs::CheckStates(void) {
  // Verify stateful partition exists.
  if (access(path_str(stateful_mount), R_OK)) {
    LOG(INFO) << stateful_mount << "does not exist.";
    return RESULT_FAIL_FATAL;
  }
  // Verify stateful is either a separate mount, or that the
  // root directory is writable (i.e. a factory install, dev mode
  // where root remounted rw, etc).
  if (same_vfs(path_str(stateful_mount), path_str(rootdir)) &&
      access(path_str(rootdir), W_OK)) {
    LOG(INFO) << stateful_mount << " is not mounted.";
    return RESULT_FAIL_FATAL;
  }

  // Verify encrypted partition is missing or not already mounted.
  if (access(path_str(encrypted_mount), R_OK) == 0 &&
      !same_vfs(path_str(encrypted_mount), path_str(stateful_mount))) {
    LOG(INFO) << encrypted_mount << " already appears to be mounted.";
    return RESULT_SUCCESS;
  }

  // Verify that bind mount targets exist.
  for (auto& bind : bind_mounts) {
    if (access(path_str(bind.dst), R_OK)) {
      PLOG(ERROR) << bind.dst << " mount point is missing.";
      return RESULT_FAIL_FATAL;
    }
  }

  // Verify that old bind mounts on stateful haven't happened yet.
  for (auto& bind : bind_mounts) {
    if (bind.submount)
      continue;

    if (same_vfs(path_str(bind.dst), path_str(stateful_mount))) {
      LOG(INFO) << bind.dst << " already bind mounted.";
      return RESULT_FAIL_FATAL;
    }
  }

  LOG(INFO) << "VFS mount state sanity check ok.";
  return RESULT_SUCCESS;
}

result_code EncryptedFs::ReportInfo(void) const {
  printf("rootdir: %s\n", path_str(rootdir));
  printf("stateful_mount: %s\n", path_str(stateful_mount));
  printf("block_path: %s\n", path_str(block_path));
  printf("encrypted_mount: %s\n", path_str(encrypted_mount));
  printf("dmcrypt_name: %s\n", dmcrypt_name.c_str());
  printf("dmcrypt_dev: %s\n", path_str(dmcrypt_dev));
  printf("bind mounts:\n");
  for (auto& mnt : bind_mounts) {
    printf("\tsrc:%s\n", path_str(mnt.src));
    printf("\tdst:%s\n", path_str(mnt.dst));
    printf("\towner:%s\n", mnt.owner.c_str());
    printf("\tmode:%o\n", mnt.mode);
    printf("\tsubmount:%d\n", mnt.submount);
    printf("\n");
  }
  return RESULT_SUCCESS;
}

brillo::SecureBlob EncryptedFs::GetKey() const {
  char* key = dm_get_key(path_str(dmcrypt_dev));
  brillo::SecureBlob encryption_key;
  if (!base::HexStringToBytes(key, &encryption_key)) {
    LOG(ERROR) << "Failed to decode encryption key.";
    return brillo::SecureBlob();
  }
  return encryption_key;
}

}  // namespace cryptohome
