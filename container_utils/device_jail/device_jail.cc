// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FUSE_USE_VERSION 29

#include <string>
#include <type_traits>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <fuse.h>
#include <fuse/cuse_lowlevel.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>

#include "container_utils/device_jail/permission_broker_client.h"

namespace dbus {
class ObjectProxy;
}

namespace device_jail {

class DeviceJail {
 public:
  DeviceJail(std::string device_path, dev_t device_number,
             PermissionBrokerClientInterface* broker_client)
      : device_path_(std::move(device_path)),
        broker_client_(broker_client) {
    jailed_device_name_ = base::StringPrintf("jailed-%d-%d",
                                             major(device_number),
                                             minor(device_number));
  }

  static DeviceJail* Get(fuse_req_t req) {
    return static_cast<DeviceJail*>(fuse_req_userdata(req));
  }

  void OpenWithBroker(const base::Callback<void(int)>& callback) {
    broker_client_->Open(device_path_, callback);
  }

  const std::string& jailed_device_name() {
    return jailed_device_name_;
  }

 private:
  const std::string device_path_;
  std::string jailed_device_name_;

  PermissionBrokerClientInterface* broker_client_;  // weak
};

static void jail_open_helper(fuse_req_t req, struct fuse_file_info fi, int fd) {
  if (fd < 0) {
    fuse_reply_err(req, -fd);
  } else {
    fi.fh = fd;
    fuse_reply_open(req, &fi);
  }
}

void jail_open(fuse_req_t req, struct fuse_file_info* fi) {
  DLOG(INFO) << "open";
  // Copy |fi| because we're doing this asynchronously and it's on the FUSE
  // message loop stack.
  DeviceJail::Get(req)->OpenWithBroker(base::Bind(&jail_open_helper, req, *fi));
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
  brillo::OpenLog("device_jail", true);
  brillo::InitLog(brillo::kLogToSyslog);

  base::CommandLine command_line(argc, argv);
  if (command_line.GetArgs().empty())
    LOG(FATAL) << "Need device to jail";
  std::string device_path = command_line.GetArgs()[0];

  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  struct stat dev_stat;
  if (stat(device_path.c_str(), &dev_stat) < 0)
    PLOG(FATAL) << "stat(" << device_path << ")";
  if (!S_ISCHR(dev_stat.st_mode))
    LOG(FATAL) << device_path << " does not describe character device";
  dev_t device_number = dev_stat.st_rdev;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  if (!bus->Connect())
    LOG(FATAL) << "D-Bus unavailable";

  dbus::ObjectProxy* broker_proxy = bus->GetObjectProxy(
      permission_broker::kPermissionBrokerServiceName,
      dbus::ObjectPath(permission_broker::kPermissionBrokerServicePath));

  auto broker_client = base::MakeUnique<device_jail::PermissionBrokerClient>(
      broker_proxy, &message_loop);
  device_jail::DeviceJail jail(device_path, device_number, broker_client.get());

  std::string cuse_devname_arg = "DEVNAME=" + jail.jailed_device_name();
  const char* dev_info_argv[] = { cuse_devname_arg.c_str() };
  struct cuse_info ci = {
    .dev_major = major(device_number),
    .dev_minor = ~minor(device_number) & 0xFFFF,
    .dev_info_argc = std::extent<decltype(dev_info_argv)>::value,
    .dev_info_argv = dev_info_argv,
    .flags = 0,
  };

  // Keep CUSE in the foreground to avoid forking and reparenting the
  // daemon.
  char foreground_opt[] = "-f";
  char* fuse_argv[] = { argv[0], foreground_opt };
  int fuse_argc = std::extent<decltype(fuse_argv)>::value;

  base::Thread cuse_thread("cuse_lowlevel_main");

  DLOG(INFO) << "Starting cuse_lowlevel_main thread";
  cuse_thread.Start();
  cuse_thread.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&cuse_lowlevel_main),
                 fuse_argc, fuse_argv, &ci, &device_jail::cops, &jail));

  message_loop.Run();
  return 0;
}
