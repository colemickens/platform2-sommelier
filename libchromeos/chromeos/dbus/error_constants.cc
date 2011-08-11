// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "error_constants.h"

GQuark chromeos_login_error_quark() {
  return g_quark_from_static_string("chromeos-login-error-quark");
}
