// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/system_utils.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <errno.h>
#include <glib.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <limits>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/scoped_temp_dir.h>
#include <base/time.h>
#include <chromeos/process.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

namespace login_manager {
//static
const char SystemUtils::kResetFile[] =
    "/mnt/stateful_partition/factory_install_reset";

SystemUtils::SystemUtils() {}
SystemUtils::~SystemUtils() {}

int SystemUtils::kill(pid_t pid, uid_t owner, int signal) {
  LOG(INFO) << "Sending " << signal << " to " << pid << " as " << owner;
  uid_t uid, euid, suid;
  getresuid(&uid, &euid, &suid);
  if (setresuid(owner, owner, -1)) {
    PLOG(ERROR) << "Couldn't assume uid " << owner;
    return -1;
  }
  int ret = ::kill(pid, signal);
  if (setresuid(uid, euid, -1)) {
    PLOG(ERROR) << "Couldn't return to root";
    return -1;
  }
  return ret;
}

bool SystemUtils::ChildIsGone(pid_t child_spec, int timeout) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeDelta max_elapsed = base::TimeDelta::FromSeconds(timeout);
  base::TimeDelta elapsed;
  int ret;

  alarm(timeout);
  do {
    errno = 0;
    ret = ::waitpid(child_spec, NULL, 0);
    elapsed = base::TimeTicks::Now() - start;
  } while (ret > 0 || (errno == EINTR && elapsed < max_elapsed));

  // Once we exit the loop, we know there was an error.
  alarm(0);
  return errno == ECHILD;  // EINTR means we timed out.
}

bool SystemUtils::EnsureAndReturnSafeFileSize(const FilePath& file,
                                              int32* file_size_32) {
  // Get the file size (must fit in a 32 bit int for NSS).
  int64 file_size;
  if (!file_util::GetFileSize(file, &file_size)) {
    LOG(ERROR) << "Could not get size of " << file.value();
    return false;
  }
  if (file_size > static_cast<int64>(std::numeric_limits<int>::max())) {
    LOG(ERROR) << file.value() << "is "
               << file_size << "bytes!!!  Too big!";
    return false;
  }
  *file_size_32 = static_cast<int32>(file_size);
  return true;
}

bool SystemUtils::EnsureAndReturnSafeSize(int64 size_64, int32* size_32) {
  if (size_64 > static_cast<int64>(std::numeric_limits<int>::max()))
    return false;
  *size_32 = static_cast<int32>(size_64);
  return true;
}

bool SystemUtils::AtomicFileWrite(const FilePath& filename,
                                  const char* data,
                                  int size) {
  FilePath scratch_file;
  if (!file_util::CreateTemporaryFileInDir(filename.DirName(), &scratch_file))
    return false;
  if (file_util::WriteFile(scratch_file, data, size) != size)
    return false;

  return (file_util::ReplaceFile(scratch_file, filename) &&
          chmod(filename.value().c_str(), (S_IRUSR | S_IWUSR | S_IROTH)) == 0);
}

bool SystemUtils::TouchResetFile() {
  FilePath reset_file(kResetFile);
  const char fast[] = "fast";
  if (file_util::WriteFile(reset_file, fast, strlen(fast)) != strlen(fast)) {
    LOG(FATAL) << "Can't write reset file to disk!!!!";
  }
  return true;
}

void SystemUtils::SendSignalToChromium(const char* signal_name,
                                       const char* payload) {
  SendSignalTo(chromium::kChromiumInterface, signal_name, payload);
}

void SystemUtils::SendSignalToPowerManager(const char* signal_name) {
  SendSignalTo(power_manager::kPowerManagerInterface, signal_name, NULL);
}

void SystemUtils::SendSignalTo(const char* interface,
                               const char* signal_name,
                               const char* payload) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              interface);
  if (!proxy) {
    LOG(ERROR) << "No proxy; can't signal " << interface;
    return;
  }
  DBusMessage* signal = ::dbus_message_new_signal("/", interface, signal_name);
  if (payload) {
    dbus_message_append_args(signal,
                             DBUS_TYPE_STRING, &payload,
                             DBUS_TYPE_INVALID);
  }
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);
}

void SystemUtils::AppendToClobberLog(const char* msg) const {
  chromeos::ProcessImpl appender;
  appender.AddArg("/sbin/clobber-log");
  appender.AddArg("--");
  appender.AddArg(msg);
  appender.Run();
}

void SystemUtils::SetGError(GError** error,
                            ChromeOSLoginError code,
                            const char* message) {
  g_set_error(error, CHROMEOS_LOGIN_ERROR, code, "Login error: %s", message);
}

void SystemUtils::SetAndSendGError(ChromeOSLoginError code,
                                   DBusGMethodInvocation* context,
                                   const char* msg) {
  GError* error = NULL;
  SetGError(&error, code, msg);
  dbus_g_method_return_error(context, error);
  g_error_free(error);
}

}  // namespace login_manager
