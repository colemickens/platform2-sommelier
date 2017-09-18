// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcontainer/libcontainer_util.h"

#include <errno.h>
#include <fcntl.h>
#if USE_device_mapper
#include <libdevmapper.h>
#endif
#include <linux/loop.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <memory>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback_helpers.h>
#include <base/files/scoped_file.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

namespace libcontainer {

namespace {

constexpr base::FilePath::CharType kLoopdevCtlPath[] =
    FILE_PATH_LITERAL("/dev/loop-control");
#if USE_device_mapper
constexpr base::FilePath::CharType kDevMapperPath[] =
    FILE_PATH_LITERAL("/dev/mapper/");
#endif

}  // namespace

int GetUsernsOutsideId(const std::string& map, int id) {
  if (map.empty())
    return id;

  std::string map_copy = map;
  base::StringPiece map_piece(map_copy);

  for (const auto& mapping : base::SplitStringPiece(
           map_piece, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    std::vector<base::StringPiece> tokens = base::SplitStringPiece(
        mapping, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (tokens.size() != 3)
      return -EINVAL;

    uint32_t inside, outside, length;
    if (!base::StringToUint(tokens[0], &inside) ||
        !base::StringToUint(tokens[1], &outside) ||
        !base::StringToUint(tokens[2], &length)) {
      return -EINVAL;
    }

    if (id >= inside && id <= (inside + length)) {
      return (id - inside) + outside;
    }
  }

  return -EINVAL;
}

int MakeDir(const base::FilePath& path, int uid, int gid, int mode) {
  if (mkdir(path.value().c_str(), mode))
    return -errno;
  if (chmod(path.value().c_str(), mode))
    return -errno;
  if (chown(path.value().c_str(), uid, gid))
    return -errno;
  return 0;
}

int TouchFile(const base::FilePath& path, int uid, int gid, int mode) {
  base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_CREAT, mode));
  if (!fd.is_valid())
    return -errno;
  if (fchown(fd.get(), uid, gid))
    return -errno;
  return 0;
}

int LoopdevSetup(const base::FilePath& source,
                 base::FilePath* loopdev_path_out) {
  base::ScopedFD source_fd(open(source.value().c_str(), O_RDONLY | O_CLOEXEC));
  if (!source_fd.is_valid())
    return -errno;

  base::ScopedFD control_fd(
      open(kLoopdevCtlPath, O_RDWR | O_NOFOLLOW | O_CLOEXEC));
  if (!control_fd.is_valid())
    return -errno;

  while (true) {
    int num = ioctl(control_fd.get(), LOOP_CTL_GET_FREE);
    if (num < 0)
      return -errno;

    base::FilePath loopdev_path(base::StringPrintf("/dev/loop%i", num));
    base::ScopedFD loop_fd(
        open(loopdev_path.value().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
    if (!loop_fd.is_valid())
      return -errno;

    if (ioctl(loop_fd.get(), LOOP_SET_FD, source_fd.get()) == 0) {
      *loopdev_path_out = loopdev_path;
      break;
    }

    if (errno != EBUSY)
      return -errno;
  }

  return 0;
}

int LoopdevDetach(const base::FilePath& loopdev) {
  base::ScopedFD fd(
      open(loopdev.value().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
  if (!fd.is_valid())
    return -errno;
  if (ioctl(fd.get(), LOOP_CLR_FD) < 0)
    return -errno;

  return 0;
}

int DeviceMapperSetup(const base::FilePath& source,
                      const std::string& verity_cmdline,
                      base::FilePath* dm_path_out,
                      std::string* dm_name_out) {
#if USE_device_mapper
  // Normalize the name into something unique-esque.
  std::string dm_name =
      base::StringPrintf("cros-containers-%s", source.value().c_str());
  base::ReplaceChars(dm_name, "/", "_", &dm_name);

  // Get the /dev path for the higher levels to mount.
  base::FilePath dm_path = base::FilePath(kDevMapperPath).Append(dm_name);

  // Insert the source path in the verity command line.
  std::string verity = verity_cmdline;
  base::ReplaceSubstringsAfterOffset(&verity, 0, "@DEV@", source.value());

  // Extract the first three parameters for dm-verity settings.
  char ttype[20];
  unsigned long long start, size;
  int n;
  if (sscanf(verity.c_str(), "%llu %llu %10s %n", &start, &size, ttype, &n) !=
      3) {
    return -errno;
  }

  /* Finally create the device mapper. */
  std::unique_ptr<struct dm_task, decltype(&dm_task_destroy)> dmt(
      dm_task_create(DM_DEVICE_CREATE), &dm_task_destroy);
  if (dmt == nullptr)
    return -errno;

  if (!dm_task_set_name(dmt.get(), dm_name.c_str()))
    return -errno;

  if (!dm_task_set_ro(dmt.get()))
    return -errno;

  if (!dm_task_add_target(dmt.get(), start, size, ttype, verity.c_str() + n))
    return -errno;

  uint32_t cookie = 0;
  if (!dm_task_set_cookie(dmt.get(), &cookie, 0))
    return -errno;

  if (!dm_task_run(dmt.get()))
    return -errno;

  /* Make sure the node exists before we continue. */
  dm_udev_wait(cookie);

  *dm_path_out = dm_path;
  *dm_name_out = dm_name;
#endif
  return 0;
}

// Tear down the device mapper target.
int DeviceMapperDetach(const std::string& dm_name) {
#if USE_device_mapper
  struct dm_task* dmt = dm_task_create(DM_DEVICE_REMOVE);
  if (dmt == nullptr)
    return -errno;

  base::ScopedClosureRunner teardown(
      base::Bind(base::IgnoreResult(&dm_task_destroy), base::Unretained(dmt)));

  if (!dm_task_set_name(dmt, dm_name.c_str()))
    return -errno;

  if (!dm_task_run(dmt))
    return -errno;
#endif
  return 0;
}

int MountExternal(const std::string& src,
                  const std::string& dest,
                  const std::string& type,
                  unsigned long flags,
                  const std::string& data) {
  bool remount_ro = false;

  // R/O bind mounts have to be remounted since 'bind' and 'ro' can't both be
  // specified in the original bind mount.  Remount R/O after the initial mount.
  if ((flags & MS_BIND) && (flags & MS_RDONLY)) {
    remount_ro = true;
    flags &= ~MS_RDONLY;
  }

  if (mount(src.c_str(),
            dest.c_str(),
            type.c_str(),
            flags,
            data.empty() ? nullptr : data.c_str()) == -1) {
    return -errno;
  }

  if (remount_ro) {
    flags |= MS_RDONLY;
    if (mount(src.c_str(),
              dest.c_str(),
              nullptr,
              flags | MS_REMOUNT,
              data.empty() ? nullptr : data.c_str()) == -1) {
      return -errno;
    }
  }

  return 0;
}

}  // namespace libcontainer
