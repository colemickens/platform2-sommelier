// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader.h"

#include <fcntl.h>
#include <linux/loop.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <sstream>
#include <string>
#include <utility>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/guid.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <dbus-c++/error.h>

#include "imageloader_common.h"

namespace imageloader {

using base::CreateDirectory;
using base::DeleteFile;
using base::FilePath;
using base::GenerateGUID;
using base::PathExists;
using base::ScopedFD;

namespace {

using imageloader::kBadResult;

// Generate a good enough (unique) mount point.
FilePath GenerateMountPoint(const char prefix[]) {
  return FilePath(prefix + GenerateGUID());
}

}  // namespace {}

// Mount component at location generated.
std::string ImageLoader::LoadComponentUtil(const std::string& name) {
  FilePath mount_point = GenerateMountPoint("/mnt/");
  // Is this somehow taken up by any other name or mount?
  for (auto it = mounts.begin(); it != mounts.end(); ++it) {
    if ((it->second).first == mount_point) {
      return kBadResult;
    }
  }
  if (PathExists(mount_point)) {
    LOG(INFO) << "Generated mount_point is already stat-able : "
              << mount_point.value();
    return kBadResult;
  }
  // The mount point is not yet taken, so go ahead.
  ScopedFD loopctl_fd(open("/dev/loop-control", O_RDONLY | O_CLOEXEC));
  if (!loopctl_fd.is_valid()) {
    PLOG(ERROR) << "loopctl_fd";
    return kBadResult;
  }
  int device_free_number = ioctl(loopctl_fd.get(), LOOP_CTL_GET_FREE);
  if (device_free_number < 0) {
    PLOG(ERROR) << "ioctl : LOOP_CTL_GET_FREE";
    return kBadResult;
  }
  std::ostringstream device_path;
  device_path << "/dev/loop" << device_free_number;
  ScopedFD device_path_fd(open(device_path.str().c_str(),
                          O_RDONLY | O_CLOEXEC));
  if (!device_path_fd.is_valid()) {
    PLOG(ERROR) << "device_path_fd";
    return kBadResult;
  }
  ScopedFD fs_image_fd(open(reg[name].second.value().c_str(),
                       O_RDONLY | O_CLOEXEC));
  if (!fs_image_fd.is_valid()) {
    PLOG(ERROR) << "fs_image_fd";
    return kBadResult;
  }
  if (ioctl(device_path_fd.get(), LOOP_SET_FD, fs_image_fd.get()) < 0) {
    PLOG(ERROR) << "ioctl: LOOP_SET_FD";
    return kBadResult;
  }
  if (!CreateDirectory(mount_point)) {
    PLOG(ERROR) << "CreateDirectory : " << mount_point.value();
    ioctl(device_path_fd.get(), LOOP_CLR_FD, 0);
    return kBadResult;
  }
  if (mount(device_path.str().c_str(), mount_point.value().c_str(), "squashfs",
            MS_RDONLY | MS_NOSUID | MS_NODEV, "") < 0) {
    PLOG(ERROR) << "mount";
    ioctl(device_path_fd.get(), LOOP_CLR_FD, 0);
    return kBadResult;
  }
  mounts[name] = std::make_pair(mount_point, FilePath(device_path.str()));
  return mount_point.value();
}

// Unmount the given component.
bool ImageLoader::UnloadComponentUtil(const std::string& name) {
  std::string device_path = mounts[name].second.value();
  if (umount(mounts[name].first.value().c_str()) < 0) {
    PLOG(ERROR) << "umount";
    return false;
  }
  const FilePath fp_mount_point(mounts[name].first);
  if (!DeleteFile(fp_mount_point, false)) {
    PLOG(ERROR) << "DeleteFile : " << fp_mount_point.value();
    return false;
  }
  ScopedFD device_path_fd(open(device_path.c_str(), O_RDONLY | O_CLOEXEC));
  if (!device_path_fd.is_valid()) {
    PLOG(ERROR) << "device_path_fd";
    return false;
  }
  if (ioctl(device_path_fd.get(), LOOP_CLR_FD, 0) < 0) {
    PLOG(ERROR) << "ioctl: LOOP_CLR_FD";
    return false;
  }
  mounts.erase(mounts.find(name));
  return true;
}

// Following functions are required directly for the DBus functionality.

ImageLoader::ImageLoader(DBus::Connection* conn)
    : DBus::ObjectAdaptor(*conn, kImageLoaderPath) {}

bool ImageLoader::RegisterComponent(const std::string& name,
                                    const std::string& version,
                                    const std::string& fs_image_abs_path,
                                    ::DBus::Error& err) {
  if (reg.find(name) == reg.end()) {
    reg[name] = std::make_pair(version, FilePath(fs_image_abs_path));
    LOG(INFO) << "Registered (" << name << ", " << version << ", "
              << fs_image_abs_path << ")";
    return true;
  }
  LOG(ERROR) <<
      "Couldn't register, entry with specified name already exists : "
      << name;
  return false;
}

std::string ImageLoader::GetComponentVersion(const std::string& name,
                                             ::DBus::Error& err) {
  if (reg.find(name) != reg.end()) {
    LOG(INFO) << "Found entry (" << name << ", " << reg[name].first << ", "
              << reg[name].second.value() << ")";
    return reg[name].first;
  }
  LOG(ERROR) << "Entry not found : " << name;
  return kBadResult;
}

std::string ImageLoader::LoadComponent(const std::string& name,
                                       ::DBus::Error& err) {
  if (reg.find(name) != reg.end()) {
    if (mounts.find(name) != mounts.end()) {
      LOG(ERROR) << "Already mounted at " << mounts[name].first.value() << ".";
      return kBadResult;
    }
    std::string mount_point = LoadComponentUtil(name);
    if (mount_point == kBadResult) {
      LOG(ERROR) << "Unable to mount : " << mount_point;
      return kBadResult;
    }
    LOG(INFO) << "Mounted successfully at " << mount_point << ".";
    return mount_point;
  }
  LOG(ERROR) << "Entry not found : " << name;
  return kBadResult;
}

bool ImageLoader::UnloadComponent(const std::string& name,
                                  ::DBus::Error& err) {
  if (UnloadComponentUtil(name)) {
    LOG(INFO) << "Unmount " << name << " successful.";
    return true;
  }
  LOG(ERROR) << "Unmount " << name << " unsucessful.";
  return false;
}

}  // namespace imageloader

int main(int argc, char **argv) {
  signal(SIGTERM, imageloader::OnQuit);
  signal(SIGINT, imageloader::OnQuit);

  DEFINE_bool(o, false, "run once");
  brillo::FlagHelper::Init(argc, argv, "imageloader");

  logging::LoggingSettings settings;
  logging::InitLogging(settings);

  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection conn = DBus::Connection::SystemBus();
  conn.request_name(imageloader::kImageLoaderName);
  imageloader::ImageLoader helper(&conn);

  if (FLAGS_o) {
    dispatcher.dispatch_pending();
  } else {
    dispatcher.enter();
  }
  return 0;
}
