// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/syslog_logging.h>
#include <libminijail.h>
#include <libusb.h>
#include <scoped_minijail.h>

#include "ippusb_manager/socket_connection.h"
#include "ippusb_manager/usb.h"

using ippusb_manager::GetUsbInfo;
using ippusb_manager::SocketConnection;
using ippusb_manager::UsbPrinterInfo;

namespace {

// Get the file descriptor of the socket created by upstart.
base::ScopedFD GetFileDescriptor() {
  const char* e = getenv("UPSTART_FDS");
  if (!e) {
    LOG(ERROR) << "No match for the environment variable \"UPSTART_FDS\"";
    exit(1);
  }

  int fd;
  if (!base::StringToInt(e, &fd)) {
    LOG(ERROR) << "Failed to parse the environment variable \"UPSTART_FDS\"";
    exit(1);
  }

  return base::ScopedFD(fd);
}

// Uses minijail to start a new instance of the XD program, using
// |socket_path| as the socket for communication, and the printer described by
// |printer_info| for printing.
void SpawnXD(const std::string& socket_path, UsbPrinterInfo* printer_info) {
  std::vector<std::string> string_args = {
      "/usr/bin/ippusbxd",
      "-d",
      "-l",
      base::StringPrintf("--bus-device=%03d:%03d", printer_info->bus(),
                         printer_info->device()),
      "--uds-path=" + socket_path,
      "--no-broadcast"
  };

  // This vector does not modify the underlying strings, it's just used for
  // compatibility with the call to execve() which libminijail makes.
  std::vector<char*> ptr_args;
  for (const std::string& s : string_args)
    ptr_args.push_back(const_cast<char*>(s.c_str()));
  ptr_args.push_back(nullptr);

  ScopedMinijail jail(minijail_new());

  // Set namespaces.
  minijail_namespace_ipc(jail.get());
  minijail_namespace_uts(jail.get());
  minijail_namespace_net(jail.get());
  // TODO(valleau): Add cgroups once devices with kernel 3.8 reach EOL.
  // crbug.com/867644
  minijail_namespace_pids(jail.get());
  minijail_namespace_vfs(jail.get());

  minijail_log_seccomp_filter_failures(jail.get());
  minijail_parse_seccomp_filters(jail.get(),
                                 "/usr/share/policy/ippusbxd-seccomp.policy");

  // Change the umask to 660 so XD will be able to write to the socket that it
  // creates.
  umask(0117);
  minijail_run(jail.get(), ptr_args[0], ptr_args.data());
}

}  // namespace

int main(int argc, char* argv[]) {
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  // Get the file descriptor of the socket created by upstart and begin
  // listening on the socket for client connections.
  SocketConnection socket_connection(GetFileDescriptor());
  if (!socket_connection.OpenSocket()) {
    LOG(ERROR) << "Failed to open socket";
    return 1;
  }

  // Since this program is only started by the upstart-socket-bridge once the
  // socket is ready to be read from, if the connection fails to open then
  // something must have gone wrong.
  if (!socket_connection.OpenConnection()) {
    LOG(ERROR) << "Failed to open connection to socket";
    return 1;
  }

  // Attempt to receive the message sent by the client.
  std::string usb_info;
  if (!socket_connection.GetMessage(&usb_info)) {
    LOG(ERROR) << "Failed to receive message";
    return 1;
  }

  // Use the message sent by the client to create a UsbPrinterInfo object.
  uint16_t vid;
  uint16_t pid;
  if (!GetUsbInfo(usb_info, &vid, &pid)) {
    LOG(ERROR) << "Failed to parse usb info string: " << usb_info;
    return 1;
  }

  auto printer_info = UsbPrinterInfo::Create(vid, pid);
  LOG(INFO) << "Received usb info: " << static_cast<int>(printer_info->vid())
            << " " << static_cast<int>(printer_info->pid());

  // Attempt to initialize the default libusb context in order to search for the
  // printer defined by |printer_info|.
  if (libusb_init(nullptr)) {
    LOG(ERROR) << "Failed to initialize libusb";
    return 1;
  }

  if (!printer_info->FindDeviceLocation()) {
    LOG(INFO) << "Couldn't find device";
    socket_connection.SendMessage("Device not found");
    socket_connection.CloseConnection();
    socket_connection.CloseSocket();
    return 0;
  }

  LOG(INFO) << "Found device on " << static_cast<int>(printer_info->bus())
            << " " << static_cast<int>(printer_info->device());

  std::string socket_name = base::StringPrintf(
      "%04x_%04x.sock", printer_info->vid(), printer_info->pid());
  std::string socket_path = "/run/ippusb/" + socket_name;

  // Only spawn a new instance of XD if there does not already exist a socket
  // with the same name.
  if (access(socket_path.c_str(), F_OK) == -1)
    SpawnXD(socket_path, printer_info.get());

  socket_connection.SendMessage(socket_name);

  socket_connection.CloseConnection();
  socket_connection.CloseSocket();
  return 0;
}
