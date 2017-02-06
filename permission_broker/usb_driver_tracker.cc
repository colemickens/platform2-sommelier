// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/usb_driver_tracker.h"

#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/message_loops/message_loop.h>

#include "permission_broker/udev_scopers.h"

using brillo::MessageLoop;

namespace permission_broker {

UsbDriverTracker::UsbDriverTracker() {}
UsbDriverTracker::~UsbDriverTracker() {
  // Re-attach all delegated USB interfaces
  for (const std::pair<int, UsbInterfaces>  &entry : dev_fds_) {
    std::string path;
    MessageLoop::TaskId tsk_id;
    std::vector<uint8_t> ifaces;
    std::tie(path, tsk_id, ifaces) = entry.second;
    MessageLoop::current()->CancelTask(tsk_id);
    ReAttachPathToKernel(path, ifaces);
  }
}

void UsbDriverTracker::ScanClosedFd(int fd) {
  auto entry = dev_fds_.find(fd);
  if (entry != dev_fds_.end()) {
    int fd = entry->first;
    int res = fcntl(fd, F_GETFL);
    if (res < 0) {  // the browser has close the file descriptor
      std::string path;
      MessageLoop::TaskId tsk_id;
      std::vector<uint8_t> ifaces;
      std::tie(path, tsk_id, ifaces) = entry->second;
      MessageLoop::current()->CancelTask(tsk_id);
      // re-attaching the kernel driver to the USB interface.
      ReAttachPathToKernel(path, ifaces);
      // we are done with the USB interface.
      dev_fds_.erase(entry);
    }
  } else {
    LOG(WARNING) << "Untracked USB file descriptor " << fd;
  }
}

bool UsbDriverTracker::ReAttachPathToKernel(const std::string& path,
    const std::vector<uint8_t>& ifaces) {
  int fd = HANDLE_EINTR(open(path.c_str(), O_RDWR));
  if (fd < 0) {
    LOG(WARNING) << "Cannot open " << path;
    return false;
  }

  for (uint8_t iface_num : ifaces) {
    struct usbdevfs_ioctl dio;
    dio.ifno = iface_num;
    dio.ioctl_code = USBDEVFS_CONNECT;
    dio.data = nullptr;

    int res = ioctl(fd, USBDEVFS_IOCTL, &dio);
    if (res < 0) {
      LOG(WARNING) << "Kernel USB driver connection for " << path
          << " on interface " << iface_num << " failed " << errno;
    } else {
      LOG(INFO) << "Kernel USB driver attached on " << path
          << " interface " << iface_num;
    }
  }
  IGNORE_EINTR(close(fd));

  return true;
}

bool UsbDriverTracker::DetachPathFromKernel(int fd, const std::string& path) {
  // Use the USB device node major/minor to find the udev entry.
  struct stat st;
  if (fstat(fd, &st) || !S_ISCHR(st.st_mode)) {
    LOG(WARNING) << "Cannot stat " << path << " device id";
    return false;
  }

  ScopedUdevPtr udev(udev_new());
  ScopedUdevDevicePtr device(
      udev_device_new_from_devnum(udev.get(), 'c', st.st_rdev));
  if (!device.get()) {
    return false;
  }

  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev.get()));
  udev_enumerate_add_match_parent(enumerate.get(), device.get());
  udev_enumerate_scan_devices(enumerate.get());

  // Try to find our USB interface nodes, by iterating through all devices
  // and extracting our children devices.
  bool detached = false;
  std::vector<uint8_t> ifaces;
  struct udev_list_entry *entry;
  udev_list_entry_foreach(entry,
                          udev_enumerate_get_list_entry(enumerate.get())) {
    const char *entry_path = udev_list_entry_get_name(entry);
    ScopedUdevDevicePtr child(udev_device_new_from_syspath(udev.get(),
                                                           entry_path));

    const char* child_type = udev_device_get_devtype(child.get());
    if (!child_type || strcmp(child_type, "usb_interface") != 0) {
      continue;
    }

    const char* driver = udev_device_get_driver(child.get());
    if (driver) {
      // A kernel driver is using this interface, try to detach it.
      const char *iface = udev_device_get_sysattr_value(
          child.get(), "bInterfaceNumber");
      unsigned iface_num;
      if (!iface || !base::StringToUint(iface, &iface_num)) {
        detached = false;
        continue;
      }

      struct usbdevfs_ioctl dio;
      dio.ifno = iface_num;
      dio.ioctl_code = USBDEVFS_DISCONNECT;
      dio.data = nullptr;

      int res = ioctl(fd, USBDEVFS_IOCTL, &dio);
      if (res < 0) {
        LOG(WARNING) << "Kernel USB driver disconnection for " << path
            << " on interface " << iface_num << " failed " << errno;
      } else {
        detached = true;
        ifaces.push_back(iface_num);
        LOG(INFO) << "USB driver '" << driver << "' detached on " << path
            << " interface " << iface_num;
      }
    }
  }

  if (detached) {
    MessageLoop::TaskId tsk_id = MessageLoop::current()->WatchFileDescriptor(
        FROM_HERE, fd, MessageLoop::WatchMode::kWatchWrite, true,
        base::Bind(&UsbDriverTracker::ScanClosedFd,
                   base::Unretained(this), fd));
    if (!tsk_id) {
      LOG(ERROR) << "Unable to watch FD " << fd;
      return true;
    }
    UsbInterfaces fd_ifaces = std::make_tuple(path, tsk_id, ifaces);
    dev_fds_[fd] = fd_ifaces;
  }

  return detached;
}

}  // namespace permission_broker
