// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/util.h"

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <dbus/dbus-glib-lowlevel.h>
#include <gdk/gdkx.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/power_constants.h"

namespace {

// Name of the X selection that the window manager takes ownership of.  This
// comes from ICCCM 4.3; see http://tronche.com/gui/x/icccm/sec-4.html#s-4.3.
const char kWindowManagerSelectionName[] = "WM_S0";

// Name of the atom used in the message_type field of X ClientEvent messages
// sent to the Chrome OS window manager.  This is hardcoded in the window
// manager.
const char kWindowManagerMessageTypeName[] = "_CHROME_WM_MESSAGE";

}  // namespace

namespace power_manager {
namespace util {

bool LoggedIn() {
  DBusGConnection* connection =
      chromeos::dbus::GetSystemBusConnection().g_connection();
  CHECK(connection);

  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      connection,
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface);

  GError* error = NULL;
  gchar* state = NULL;
  gchar* user = NULL;
  bool retval;
  if (dbus_g_proxy_call(proxy,
                        login_manager::kSessionManagerRetrieveSessionState,
                        &error,
                        G_TYPE_INVALID,
                        G_TYPE_STRING,
                        &state,
                        G_TYPE_STRING,
                        &user,
                        G_TYPE_INVALID)) {
    retval = strcmp(state, "started") == 0;
    g_free(state);
    g_free(user);
  } else {
    LOG(ERROR) << "Unable to retrieve session state from session manager: "
               << error->message;
    g_error_free(error);
    retval = access("/var/run/state/logged-in", F_OK) == 0;
  }
  g_object_unref(proxy);
  return retval;
}

bool OOBECompleted() {
  return access("/home/chronos/.oobe_completed", F_OK) == 0;
}

void Launch(const char* cmd) {
  LOG(INFO) << "Launching " << cmd;
  pid_t pid = fork();
  if (pid == 0) {
    // Detach from parent so that powerd doesn't need to wait around for us
    setsid();
    exit(fork() == 0 ? system(cmd) : 0);
  } else if (pid > 0) {
    waitpid(pid, NULL, 0);
  }
}

// A utility function to send a signal to the session manager.
void SendSignalToSessionManager(const char* signal) {
  DBusGConnection* connection;
  GError* error = NULL;
  connection = chromeos::dbus::GetSystemBusConnection().g_connection();
  CHECK(connection);
  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      connection,
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface);
  CHECK(proxy);
  error = NULL;
  if (!dbus_g_proxy_call(proxy, signal, &error, G_TYPE_INVALID,
                         G_TYPE_INVALID)) {
    LOG(ERROR) << "Error sending signal: " << error->message;
    g_error_free(error);
  }
  g_object_unref(proxy);
}

// A utility function to send a signal to the lower power daemon (powerm).
void SendSignalToPowerM(const char* signal_name) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              kRootPowerManagerInterface);
  DBusMessage* signal = ::dbus_message_new_signal(
      "/",
      kRootPowerManagerInterface,
      signal_name);
  CHECK(signal);
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);
}

// A utility function to send a signal to upper power daemon (powerd).
void SendSignalToPowerD(const char* signal_name) {
  LOG(INFO) << "Sending signal '" << signal_name << "' to PowerManager:";
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              kPowerManagerInterface);
  DBusMessage* signal = ::dbus_message_new_signal(
      "/",
      kPowerManagerInterface,
      signal_name);
  CHECK(signal);
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);
}

void CreateStatusFile(const FilePath& file) {
  if (!file_util::WriteFile(file, NULL, 0) == -1)
    LOG(ERROR) << "Unable to create " << file.value();
  else
    LOG(INFO) << "Created " << file.value();
}

void RemoveStatusFile(const FilePath& file) {
  if (file_util::PathExists(file)) {
    if (!file_util::Delete(file, FALSE))
      LOG(ERROR) << "Unable to remove " << file.value();
    else
      LOG(INFO) << "Removed " << file.value();
  }
}

bool SendMessageToWindowManager(chromeos::WmIpcMessageType type,
                                int first_param) {
  // Ensure that we won't crash if we get an error from the X server.
  gdk_error_trap_push();

  Display* display = GDK_DISPLAY();
  Window wm_window = XGetSelectionOwner(
      display, XInternAtom(display, kWindowManagerSelectionName, True));
  if (wm_window == None) {
    LOG(WARNING) << "Unable to find window owning the "
                 << kWindowManagerSelectionName << " X selection -- is the "
                 << "window manager running?";
    gdk_error_trap_pop();
    return false;
  }

  XEvent event;
  event.xclient.type = ClientMessage;
  event.xclient.window = wm_window;
  event.xclient.message_type =
      XInternAtom(display, kWindowManagerMessageTypeName, True);
  event.xclient.format = 32;  // 32-bit values
  event.xclient.data.l[0] = type;
  event.xclient.data.l[1] = first_param;
  for (int i = 2; i < 5; ++i)
    event.xclient.data.l[i] = 0;
  XSendEvent(display,
             wm_window,
             False,  // propagate
             0,      // empty event mask
             &event);

  gdk_flush();
  if (gdk_error_trap_pop()) {
    LOG(WARNING) << "Got error while sending message to window manager";
    return false;
  }
  return true;
}

}  // namespace util
}  // namespace power_manager
