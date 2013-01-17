// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/system_utils.h"

#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <limits>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/scoped_temp_dir.h>
#include <base/time.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/process.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>

#include "login_manager/scoped_dbus_pending_call.h"

using std::string;
using std::vector;

namespace login_manager {
namespace gobject {
struct SessionManager;
}

const char SystemUtils::kSignalSuccess[] = "success";
const char SystemUtils::kSignalFailure[] = "failure";

SystemUtils::SystemUtils() {}
SystemUtils::~SystemUtils() {}

int SystemUtils::IsDevMode() {
  int dev_mode_code = system("crossystem 'cros_debug?0'");
  if (WIFEXITED(dev_mode_code)) {
    return WEXITSTATUS(dev_mode_code);
  }
  return -1;
}

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

time_t SystemUtils::time(time_t* t) {
  return ::time(t);
}

pid_t SystemUtils::fork() {
  return ::fork();
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

bool SystemUtils::Exists(const FilePath& file) {
  return file_util::PathExists(file);
}

bool SystemUtils::GetUniqueFilenameInWriteOnlyTempDir(
    FilePath* temp_file_path) {
  // Create a temporary directory to put the testing channel in.
  // It will be made write-only below; we need to be able to read it
  // when trying to create a unique name inside it.
  FilePath temp_dir_path;
  if (!file_util::CreateNewTempDirectory(
          FILE_PATH_LITERAL(""), &temp_dir_path)) {
    PLOG(ERROR) << "Can't create temp dir";
    return false;
  }
  // Create a temporary file in the temporary directory, to be deleted later.
  // This ensures a unique name.
  if (!file_util::CreateTemporaryFileInDir(temp_dir_path, temp_file_path)) {
    PLOG(ERROR) << "Can't get temp file name in " << temp_dir_path.value();
    return false;
  }
  // Now, allow access to non-root processes.
  if (chmod(temp_dir_path.value().c_str(), 0333)) {
    PLOG(ERROR) << "Can't chmod " << temp_file_path->value() << " to 0333";
    return false;
  }
  if (!RemoveFile(*temp_file_path)) {
    PLOG(ERROR) << "Can't clear temp file in " << temp_file_path->value();
    return false;
  }
  return true;
}

bool SystemUtils::RemoveFile(const FilePath& filename) {
  return file_util::Delete(filename, false);
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

void SystemUtils::EmitSignal(const char* signal_name) {
  EmitSignalWithStringArgs(signal_name, vector<string>());
}

void SystemUtils::EmitSignalWithStringArgs(const char* signal_name,
                                           const vector<string>& payload) {
  EmitSignalFrom(chromium::kChromiumInterface, signal_name, payload);
  EmitSignalFrom(login_manager::kSessionManagerInterface, signal_name, payload);
}

void SystemUtils::EmitStatusSignal(const char* signal_name, bool status) {
  EmitSignalFrom(chromium::kChromiumInterface, signal_name,
                 vector<string>(1, status ? kSignalSuccess : kSignalFailure));
  EmitSignalFrom(login_manager::kSessionManagerInterface, signal_name,
                 vector<string>(1, status ? kSignalSuccess : kSignalFailure));
}

void SystemUtils::CallMethodOnPowerManager(const char* method_name) {
  CallMethodOn(power_manager::kPowerManagerServiceName,
               power_manager::kPowerManagerServicePath,
               power_manager::kPowerManagerInterface,
               method_name);
}

scoped_ptr<ScopedDBusPendingCall> SystemUtils::CallAsyncMethodOnChromium(
    const char* method_name) {
  DBusMessage* method = dbus_message_new_method_call(
      chromeos::kLibCrosServiceName,
      chromeos::kLibCrosServicePath,
      chromeos::kLibCrosServiceInterface,
      method_name);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  DBusPendingCall* to_return = NULL;
  // Only fails on OOM conditions.
  CHECK(dbus_connection_send_with_reply(connection, method, &to_return, -1));
  dbus_message_unref(method);
  return ScopedDBusPendingCall::Create(to_return);
}

bool SystemUtils::CheckAsyncMethodSuccess(DBusPendingCall* pending_call) {
  DCHECK(pending_call);
  // This returns NULL if |pending_call| hasn't completed yet.
  DBusMessage* reply = dbus_pending_call_steal_reply(pending_call);
  if (!reply) {
    CancelAsyncMethodCall(pending_call);
    return false;
  }

  DBusError error;
  dbus_error_init(&error);
  // By definition, getting a method_return message means the async
  // method completed successfully.
  bool to_return =
      dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN;
  if (dbus_set_error_from_message(&error, reply))
    LOG(ERROR) << "Liveness ping resulted in an error: " << error.message;

  if (dbus_error_is_set(&error))
    dbus_error_free(&error);
  dbus_message_unref(reply);
  return to_return;
}

void SystemUtils::CancelAsyncMethodCall(DBusPendingCall* pending_call) {
  DCHECK(pending_call);
  dbus_pending_call_cancel(pending_call);
}

void SystemUtils::EmitSignalFrom(const char* interface,
                                 const char* signal_name,
                                 const vector<string>& payload) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              login_manager::kSessionManagerServicePath,
                              interface);
  if (!proxy) {
    LOG(ERROR) << "No proxy; can't signal " << interface;
    return;
  }
  DBusMessage* signal = ::dbus_message_new_signal(
      login_manager::kSessionManagerServicePath, interface, signal_name);
  if (!payload.empty()) {
    for (vector<string>::const_iterator it = payload.begin();
         it != payload.end();
         ++it) {
      // Need to be able to take the address of the value to append.
      const char* string_arg = it->c_str();
      dbus_message_append_args(signal,
                               DBUS_TYPE_STRING, &string_arg,
                               DBUS_TYPE_INVALID);
    }
  }
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);
}

void SystemUtils::CallMethodOn(const char* destination,
                               const char* path,
                               const char* interface,
                               const char* method_name) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              destination, path, interface);
  if (!proxy) {
    LOG(ERROR) << "No proxy; can't call " << interface << "." << method_name;
    return;
  }
  GError* error = NULL;
  ::dbus_g_proxy_call(proxy.gproxy(), method_name, &error, G_TYPE_INVALID,
                      G_TYPE_INVALID);
  if (error)
    LOG(ERROR) << interface << "." << path << " failed: " << error->message;
}

void SystemUtils::AppendToClobberLog(const char* msg) const {
  chromeos::ProcessImpl appender;
  appender.AddArg("/sbin/clobber-log");
  appender.AddArg("--");
  appender.AddArg(msg);
  appender.Run();
}

void SystemUtils::SetAndSendGError(ChromeOSLoginError code,
                                   DBusGMethodInvocation* context,
                                   const char* msg) {
  GError* error = NULL;
  g_set_error(&error, CHROMEOS_LOGIN_ERROR, code, "Login error: %s", msg);
  dbus_g_method_return_error(context, error);
  g_error_free(error);
}

}  // namespace login_manager
