// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SUSPENDER_H_
#define POWER_MANAGER_POWERD_SUSPENDER_H_

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <map>
#include <string>

#include "base/file_path.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/screen_locker.h"

namespace power_manager {

class Daemon;
class FileTagger;

class Suspender {
 public:
  Suspender(ScreenLocker* locker, FileTagger* file_tagger);

  void Init(const FilePath& run_dir, Daemon* daemon);

  // Suspend the computer, locking the screen first.
  void RequestSuspend();

  // Check whether the computer should be suspended. Before calling this
  // method, the screen should be locked.
  void CheckSuspend();

  // Cancel Suspend in progress.
  void CancelSuspend();

  // Handles a RegisterSuspendDelay request.  The caller is responsible for
  // sending the returned reply and freeing it with dbus_message_unref().
  DBusMessage* RegisterSuspendDelay(DBusMessage* message);

  // Handles an UnregisterSuspendDelay request.  The caller is responsible for
  // sending the returned reply and freeing it with dbus_message_unref().
  DBusMessage* UnregisterSuspendDelay(DBusMessage* message);

 private:
  // Handle SuspendReady Dbus Messages.
  void SuspendReady(DBusMessage* message);

  // Standard handler for dbus messages. |data| contains a pointer to a
  // Daemon object.
  static DBusHandlerResult DBusMessageHandler(DBusConnection*,
                                              DBusMessage* message,
                                              void* data);

  // Register the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Suspend the computer. Before calling this method, the screen should
  // be locked.
  void Suspend();

  // Timeout callback in case suspend clients do not respond in time.
  // Always returns false.
  SIGNAL_CALLBACK_PACKED_1(Suspender, gboolean, CheckSuspendTimeout,
                           unsigned int);

  static void NameOwnerChangedHandler(DBusGProxy* proxy,
                                      const gchar* name,
                                      const gchar* old_owner,
                                      const gchar* new_owner,
                                      gpointer data);

  // Clean up suspend delay upon unregister or dbus name change.
  // Remove |client_name| from list of suspend delay callback clients.
  bool CleanUpSuspendDelay(const char* client_name);

  void BroadcastSignalToClients(const char* signal_name,
                                const unsigned int sequence_num);

  // Reference to ScreenLocker object.
  ScreenLocker* locker_;

  // Reference to FileTagger object.
  FileTagger* file_tagger_;

  DBusConnection* connection_;

  // Whether the computer should be suspended soon.
  unsigned int suspend_delay_timeout_ms_;
  unsigned int suspend_delays_outstanding_;
  bool suspend_requested_;
  unsigned int suspend_sequence_number_;

  // Identify user activity to cancel suspend in progress.
  FilePath user_active_file_;

  // suspend_delays_ : map from dbus unique identifiers to expected ms delays.
  typedef std::map<std::string, uint32> SuspendList;
  SuspendList suspend_delays_;

  // Number of wake events received at start of current suspend operation
  unsigned int wakeup_count_;
  bool wakeup_count_valid_;

  // Reference to the owning daemon, so that this class can callback to it
  Daemon* daemon_;

  DISALLOW_COPY_AND_ASSIGN(Suspender);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SUSPENDER_H_
