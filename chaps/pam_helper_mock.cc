// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/pam_helper_mock.h"

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include "chaps/chaps_utility.h"

// Create PAM stubs to keep the linker happy when building the tests.

int pam_get_user(pam_handle_t* pamh, const char** user, const char* prompt) {
  NOTREACHED();
  return 0;
}

int pam_get_item(const pam_handle_t* pamh, int item_type, const void** item) {
  NOTREACHED();
  return 0;
}

const char* pam_strerror(pam_handle_t* pamh, int errnum) {
  NOTREACHED();
  return NULL;
}

int pam_set_data(pam_handle_t* pamh,
                 const char* module_data_name,
                 void* data,
                 void (*cleanup)(pam_handle_t* pamh,
                                 void* data,
                                 int error_status)) {
  NOTREACHED();
  return 0;
}

int pam_get_data(const pam_handle_t* pamh,
                 const char* module_data_name,
                 const void** data) {
  NOTREACHED();
  return 0;
}

int pam_putenv(pam_handle_t* pamh, const char* name_value) {
  NOTREACHED();
  return 0;
}

const char* pam_getenv(pam_handle_t* pamh, const char* name) {
  NOTREACHED();
  return NULL;
}
