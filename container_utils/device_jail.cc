// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FUSE_USE_VERSION 29

#include <string>
#include <type_traits>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <dbus/bus.h>
#include <dbus/file_descriptor.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>
#include <fuse.h>
#include <fuse/cuse_lowlevel.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

#include <chromeos/dbus/service_constants.h>

namespace device_jail {

class DeviceJail {
 public:
  DeviceJail(std::string device_path, dev_t device_number)
      : device_path_(std::move(device_path)) {
    jailed_device_name_ = base::StringPrintf("jailed-%d-%d",
                                             major(device_number),
                                             minor(device_number));
  }

  static int OpenWithBroker(fuse_req_t req) {
    const std::string& path =
        static_cast<DeviceJail*>(fuse_req_userdata(req))->device_path_;
    DLOG(INFO) << "OpenWithBroker(" << path << ")";

    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
    if (!bus->Connect()) {
      LOG(ERROR) << "OpenWithBroker(" << path << "): D-Bus unavailable";
      return -EINTR;
    }
    dbus::ObjectProxy* proxy = bus->GetObjectProxy(
        permission_broker::kPermissionBrokerServiceName,
        dbus::ObjectPath(permission_broker::kPermissionBrokerServicePath));

    dbus::MethodCall method_call(permission_broker::kPermissionBrokerInterface,
                                 permission_broker::kOpenPath);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);

    std::unique_ptr<dbus::Response> response =
        proxy->CallMethodAndBlock(&method_call,
                                  dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
    if (!response) {
      DLOG(INFO) << "OpenWithBroker(" << path << "): permission denied";
      return -EACCES;
    }

    dbus::FileDescriptor fd;
    dbus::MessageReader reader(response.get());
    if (!reader.PopFileDescriptor(&fd)) {
      LOG(ERROR) << "Could not parse permission broker's response";
      return -EINVAL;
    }

    fd.CheckValidity();
    if (!fd.is_valid()) {
      LOG(ERROR) << "Permission broker returned invalid fd";
      return -EINVAL;
    }

    DLOG(INFO) << "OpenWithBroker(" << path << ") -> " << fd.value();
    return fd.TakeValue();
  }

  const std::string& jailed_device_name() {
    return jailed_device_name_;
  }

 private:
  const std::string device_path_;
  std::string jailed_device_name_;
};

void jail_open(fuse_req_t req, struct fuse_file_info* fi) {
  DLOG(INFO) << "open";
  int fd = DeviceJail::OpenWithBroker(req);
  if (fd < 0) {
    fuse_reply_err(req, -fd);
  } else {
    fi->fh = fd;
    fuse_reply_open(req, fi);
  }
}

void jail_read(fuse_req_t req, size_t size, off_t off,
               struct fuse_file_info* fi) {
  DLOG(INFO) << "read(" << size << ")";
  std::vector<char> buf(size);
  // Ignore |off| because character devices are not seekable and
  // CUSE always passes in 0.
  int ret = read(fi->fh, buf.data(), size);
  if (ret < 0)
    fuse_reply_err(req, errno);
  else
    fuse_reply_buf(req, buf.data(), ret);
}

void jail_write(fuse_req_t req, const char* buf, size_t size, off_t off,
                struct fuse_file_info* fi) {
  DLOG(INFO) << "write(" << size << ")";
  // Ignore |off| (see comment in jail_read).
  int ret = write(fi->fh, buf, size);
  if (ret < 0)
    fuse_reply_err(req, errno);
  else
    fuse_reply_write(req, ret);
}

void jail_release(fuse_req_t req, struct fuse_file_info* fi) {
  DLOG(INFO) << "close";
  fuse_reply_err(req, close(fi->fh) < 0 ? errno : 0);
}

void jail_ioctl(fuse_req_t req, int cmd, void* arg,
                struct fuse_file_info* fi, unsigned int flags,
                const void *in_buf, size_t in_bufsz, size_t out_bufsz) {
  DLOG(INFO) << "ioctl(" << cmd << ")";
  if (flags & FUSE_IOCTL_COMPAT) {
    fuse_reply_err(req, ENOSYS);
    return;
  }

  // We are using restricted ioctls because there's no way to get
  // enough information to reliably pass through unrestricted ioctls.
  // This means all the direction and size information is encoded in
  // the ioctl number, and the kernel has already set up the necessary
  // storage in *arg.
  int ret = ioctl(fi->fh, cmd, arg);
  if (ret < 0) {
    fuse_reply_err(req, errno);
  } else {
    if (_IOC_DIR(cmd) & _IOC_WRITE)
      fuse_reply_ioctl(req, ret, arg, _IOC_SIZE(cmd));
    else
      fuse_reply_ioctl(req, ret, nullptr, 0);
  }
}

struct cuse_lowlevel_ops cops = {
  .init = nullptr,
  .init_done = nullptr,
  .destroy = nullptr,
  .open = jail_open,
  .read = jail_read,
  .write = jail_write,
  .flush = nullptr,
  .release = jail_release,
  .fsync = nullptr,
  .ioctl = jail_ioctl,
  .poll = nullptr,
};

}  // namespace device_jail

int main(int argc, char** argv) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  char* device_path;
  if (fuse_parse_cmdline(&args, &device_path, nullptr, nullptr) < 0)
    LOG(FATAL) << "Failed to parse command line";
  if (!device_path)
    LOG(FATAL) << "Need device to jail";

  struct stat dev_stat;
  if (stat(device_path, &dev_stat) < 0)
    PLOG(FATAL) << "stat";
  if (!S_ISCHR(dev_stat.st_mode))
    LOG(FATAL) << device_path << " does not describe character device";

  brillo::OpenLog("device_jail", true);
  brillo::InitLog(brillo::kLogToSyslog);

  device_jail::DeviceJail jail(device_path, dev_stat.st_rdev);

  std::string cuse_devname_arg = "DEVNAME=" + jail.jailed_device_name();
  const char* dev_info_argv[] = { cuse_devname_arg.c_str() };
  struct cuse_info ci = {
    .dev_major = major(dev_stat.st_rdev),
    .dev_minor = ~minor(dev_stat.st_rdev) & 0xFFFF,
    .dev_info_argc = std::extent<decltype(dev_info_argv)>::value,
    .dev_info_argv = dev_info_argv,
    .flags = 0,
  };

  int ret = cuse_lowlevel_main(argc, argv, &ci, &device_jail::cops, &jail);

  free(device_path);
  fuse_opt_free_args(&args);
  return ret;
}
