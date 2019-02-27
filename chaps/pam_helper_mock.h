// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_PAM_HELPER_MOCK_H_
#define CHAPS_PAM_HELPER_MOCK_H_

#include "chaps/pam_helper.h"

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace chaps {

class PamHelperMock : public PamHelper {
 public:
  MOCK_METHOD2(GetPamUser, bool(pam_handle_t*, std::string*));

  MOCK_METHOD3(GetPamPassword,
               bool(pam_handle_t*, bool old_password, brillo::SecureBlob*));
  MOCK_METHOD3(SaveUserAndPassword,
               bool(pam_handle_t*,
                    const std::string&,
                    const brillo::SecureBlob&));
  MOCK_METHOD3(RetrieveUserAndPassword,
               bool(pam_handle_t*, std::string*, brillo::SecureBlob*));
  MOCK_METHOD3(PutEnvironmentVariable,
               bool(pam_handle_t*, const std::string&, const std::string&));
  MOCK_METHOD3(GetEnvironmentVariable,
               bool(pam_handle_t*, const std::string&, std::string*));
};

}  // namespace chaps

#endif  // CHAPS_PAM_HELPER_MOCK_H_
