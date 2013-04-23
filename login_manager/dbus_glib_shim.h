// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef LOGIN_MANAGER_DBUS_GLIB_SHIM_H_
#define LOGIN_MANAGER_DBUS_GLIB_SHIM_H_

#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib.h>
#include <glib-object.h>
#include <stdlib.h>

#include <base/logging.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/glib/object.h>

// Helpers for using GObjects until we can get a C++ wrapper going.
namespace login_manager {
class SessionManagerInterface;

namespace gobject {  // Namespace hiding the GObject type data.

struct SessionManager {
  GObject parent_instance;
  SessionManagerInterface *impl;  // pointer to SessionManager API implemention.
};
struct SessionManagerClass { GObjectClass parent_class; };

// session_manager_get_type() is defined in dbus_glib_shim.cc by the
// G_DEFINE_TYPE() macro.  This macro defines a number of other GLib
// class system specific functions and variables discussed in
// dbus_glib_shim.cc.
GType session_manager_get_type();  // defined by G_DEFINE_TYPE
struct dbus_glib_session_manager_object_info;  // defined by G_DEFINE_TYPE

// Interface function prototypes which wrap impl.
gboolean session_manager_emit_login_prompt_ready(SessionManager *self,
                                                 gboolean *OUT_emitted,
                                                 GError **error);
gboolean session_manager_emit_login_prompt_visible(SessionManager* self,
                                                   GError** error);
gboolean session_manager_enable_chrome_testing(SessionManager* self,
                                               gboolean force_relaunch,
                                               const gchar** extra_arguments,
                                               gchar** OUT_filepath,
                                               GError** error);
gboolean session_manager_start_session(SessionManager *self,
                                       gchar *email_address,
                                       gchar *unique_identifier,
                                       gboolean *OUT_done,
                                       GError **error);
gboolean session_manager_stop_session(SessionManager *self,
                                      gchar *unique_identifier,
                                      gboolean *OUT_done,
                                      GError **error);

gboolean session_manager_store_policy(SessionManager *self,
                                      GArray *policy_blob,
                                      DBusGMethodInvocation* context);
gboolean session_manager_retrieve_policy(SessionManager *self,
                                         GArray **OUT_policy_blob,
                                         GError **error);
gboolean session_manager_store_user_policy(SessionManager *self,
                                           GArray *policy_blob,
                                           DBusGMethodInvocation* context);
gboolean session_manager_retrieve_user_policy(SessionManager *self,
                                              GArray **OUT_policy_blob,
                                              GError **error);
gboolean session_manager_store_policy_for_user(SessionManager *self,
                                               gchar* user_email,
                                               GArray *policy_blob,
                                               DBusGMethodInvocation* context);
gboolean session_manager_retrieve_policy_for_user(SessionManager *self,
                                                  gchar* user_email,
                                                  GArray **OUT_policy_blob,
                                                  GError **error);
gboolean session_manager_store_device_local_account_policy(
    SessionManager *self,
    gchar* account_id,
    GArray *policy_blob,
    DBusGMethodInvocation* context);
gboolean session_manager_retrieve_device_local_account_policy(
    SessionManager *self,
    gchar* account_id,
    GArray **OUT_policy_blob,
    GError **error);
gboolean session_manager_retrieve_session_state(SessionManager *self,
                                                gchar** OUT_state);

gboolean session_manager_unlock_screen(SessionManager *self,
                                       GError **error);
gboolean session_manager_handle_lock_screen_dismissed(SessionManager *self,
                                                      GError **error);
gboolean session_manager_lock_screen(SessionManager *self,
                                     GError **error);
gboolean session_manager_handle_lock_screen_shown(SessionManager *self,
                                                  GError **error);

gboolean session_manager_restart_job(SessionManager *self,
                                     gint pid,
                                     gchar *arguments,
                                     gboolean *OUT_done,
                                     GError **error);
gboolean session_manager_restart_job_with_auth(SessionManager *self,
                                               gint pid,
                                               gchar *cookie,
                                               gchar *arguments,
                                               gboolean *OUT_done,
                                               GError **error);
gboolean session_manager_start_device_wipe(SessionManager *self,
                                           gboolean *OUT_done,
                                           GError **error);
}  // namespace gobject
}  // namespace login_manager
#endif  // LOGIN_MANAGER_DBUS_GLIB_SHIM_H_
