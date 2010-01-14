// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef LOGIN_MANAGER_INTERFACE_H_
#define LOGIN_MANAGER_INTERFACE_H_

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <glib-object.h>
#include <stdlib.h>

#include <base/logging.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/glib/object.h>

#include "login_manager/session_manager_service.h"

// Helpers for using GObjects until we can get a C++ wrapper going.
namespace login_manager {
namespace gobject {  // Namespace hiding the GObject type data.

struct SessionManager {
  GObject parent_instance;
  SessionManagerService *service;  // pointer to implementing service.
};
struct SessionManagerClass { GObjectClass parent_class; };

// session_manager_get_type() is defined in interface.cc by the
// G_DEFINE_TYPE() macro.  This macro defines a number of other GLib
// class system specific functions and variables discussed in
// interface.cc.
GType session_manager_get_type();  // defined by G_DEFINE_TYPE

// Interface function prototypes which wrap service.
gboolean session_manager_emit_login_prompt_ready(SessionManager *self,
                                                 gboolean *OUT_emitted,
                                                 GError **error);
gboolean session_manager_start_session(SessionManager *self,
                                       gchar *email_address,
                                       gchar *unique_identifier,
                                       gboolean *OUT_done,
                                       GError **error);
gboolean session_manager_stop_session(SessionManager *self,
                                      gchar *unique_identifier,
                                      gboolean *OUT_done,
                                      GError **error);
}  // namespace gobject
}  // namespace login_manager
#endif  // LOGIN_MANAGER_INTERFACE_H_
