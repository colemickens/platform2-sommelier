// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PROPERTY_STORE_
#define SHILL_MOCK_PROPERTY_STORE_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/property_store.h"

namespace shill {

class MockPropertyStore : public PropertyStore {
 public:
  MockPropertyStore();
  virtual ~MockPropertyStore();

  MOCK_CONST_METHOD1(Contains, bool(const std::string&));
  MOCK_METHOD3(SetBoolProperty, bool(const std::string&, bool, Error*));
  MOCK_METHOD3(SetInt16Property, bool(const std::string&, int16, Error*));
  MOCK_METHOD3(SetInt32Property, bool(const std::string&, int32, Error*));
  MOCK_METHOD3(SetStringProperty, bool(const std::string&,
                                       const std::string&,
                                       Error*));
  MOCK_METHOD3(SetStringmapProperty, bool(const std::string&,
                                          const Stringmap&,
                                          Error*));
  MOCK_METHOD3(SetStringsProperty, bool(const std::string&,
                                        const Strings&,
                                        Error*));
  MOCK_METHOD3(SetUint8Property, bool(const std::string&, uint8, Error*));
  MOCK_METHOD3(SetUint16Property, bool(const std::string&, uint16, Error*));
  MOCK_METHOD3(SetUint32Property, bool(const std::string&, uint32, Error*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPropertyStore);
};

}  // namespace shill

#endif  // SHILL_MOCK_PROPERTY_STORE_
