// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pkcs11Init - class for handling opencryptoki initialization.

#ifndef CRYPTOHOME_PKCS11_INIT_H_
#define CRYPTOHOME_PKCS11_INIT_H_

#include <base/basictypes.h>
#include <glib.h>

namespace cryptohome {

class Pkcs11Init {
 public:
  Pkcs11Init();
  virtual ~Pkcs11Init();

  bool IsTpmTokenReady() const {
    return true;
  }
  void GetTpmTokenInfo(gchar **OUT_label,
                       gchar **OUT_user_pin);

 private:
  DISALLOW_COPY_AND_ASSIGN(Pkcs11Init);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_PKCS11_INIT_H_
