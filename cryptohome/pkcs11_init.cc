// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Pkcs11Init

#include "pkcs11_init.h"

#include <iostream>

#include <base/logging.h>
#include <base/file_util.h>
#include <base/string_util.h>
#include <glib.h>
#include <opencryptoki/pkcs11.h>

#include "platform.h"

namespace cryptohome {

const CK_CHAR kDefaultUserPin[] = "111111";
const CK_CHAR kDefaultLabel[] = "TPM";

Pkcs11Init::Pkcs11Init() {
}

Pkcs11Init::~Pkcs11Init() {
}

void Pkcs11Init::GetTpmTokenInfo(gchar **OUT_label,
                                 gchar **OUT_user_pin) {
  *OUT_label = g_strdup(reinterpret_cast<const gchar *>(kDefaultLabel));
  *OUT_user_pin = g_strdup(reinterpret_cast<const gchar *>(kDefaultUserPin));
}

}  // namespace cryptohome
