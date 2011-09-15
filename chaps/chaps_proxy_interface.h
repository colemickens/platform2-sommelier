// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHAPS_CHAPS_PROXY_INTERFACE_H
#define CHAPS_CHAPS_PROXY_INTERFACE_H

#include <string>
#include <vector>

#include <base/basictypes.h>

namespace chaps {

// ChapsProxyInterface provides an abstract interface to the proxy to
// facilitate mocking. It is based on the dbus-c++ generated interface. See
// chaps_interface.xml.
class ChapsProxyInterface {
public:
  ChapsProxyInterface() {}
  virtual ~ChapsProxyInterface() {}

  // The Connect method establishes a connection with the Chaps daemon. It
  // must be called before any other methods. The return value is true on
  // success.
  virtual bool Connect(const std::string& username) = 0;
  // The Disconnect method closes a connection with the Chaps daemon. It should
  // be called when the interface is no longer needed.
  virtual void Disconnect() = 0;

  // The following methods map to PKCS #11 calls. Each method name is identical
  // to the corresponding PKCS #11 function name except for the "C_" prefix.

  // PKCS #11 v2.20 section 11.5 page 106.
  virtual uint32_t GetSlotList(const bool& token_present,
                               std::vector<uint32_t>& slot_list) = 0;
  // PKCS #11 v2.20 section 11.5 page 108.
  virtual uint32_t GetSlotInfo(const uint32_t& slot_id,
                               std::string& slot_description,
                               std::string& manufacturer_id,
                               uint32_t& flags,
                               uint8_t& hardware_version_major,
                               uint8_t& hardware_version_minor,
                               uint8_t& firmware_version_major,
                               uint8_t& firmware_version_minor) = 0;

private:
  DISALLOW_COPY_AND_ASSIGN(ChapsProxyInterface);
};

}  // namespace

#endif  // CHAPS_CHAPS_PROXY_INTERFACE_H

