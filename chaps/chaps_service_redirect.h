// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHAPS_CHAPS_SERVICE_REDIRECT_H
#define CHAPS_CHAPS_SERVICE_REDIRECT_H

#include "chaps/chaps_interface.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

// ChapsServiceRedirect simply redirects calls to a PKCS #11 library.
class ChapsServiceRedirect : public ChapsInterface {
public:
  explicit ChapsServiceRedirect(const char* library_path);
  virtual ~ChapsServiceRedirect();
  bool Init();
  void TearDown();
  // ChapsInterface methods
  virtual uint32_t GetSlotList(bool token_present,
                               std::vector<uint32_t>* slot_list);
  virtual uint32_t GetSlotInfo(uint32_t slot_id,
                               std::string* slot_description,
                               std::string* manufacturer_id,
                               uint32_t* flags,
                               uint8_t* hardware_version_major,
                               uint8_t* hardware_version_minor,
                               uint8_t* firmware_version_major,
                               uint8_t* firmware_version_minor);
private:
  std::string library_path_;
  void* library_;
  CK_FUNCTION_LIST_PTR functions_;

  DISALLOW_COPY_AND_ASSIGN(ChapsServiceRedirect);
};

}  // namespace
#endif  // CHAPS_CHAPS_SERVICE_REDIRECT_H
