// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_MOCK_UDEV_LIST_ENTRY_H_
#define MIST_MOCK_UDEV_LIST_ENTRY_H_

#include <base/compiler_specific.h>
#include <gmock/gmock.h>

#include "mist/udev_list_entry.h"

namespace mist {

class MockUdevListEntry : public UdevListEntry {
 public:
  MockUdevListEntry();
  virtual ~MockUdevListEntry() OVERRIDE;

  MOCK_CONST_METHOD0(GetNext, UdevListEntry*());
  MOCK_CONST_METHOD1(GetByName, UdevListEntry*(const char* name));
  MOCK_CONST_METHOD0(GetName, const char*());
  MOCK_CONST_METHOD0(GetValue, const char*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUdevListEntry);
};

}  // namespace mist

#endif  // MIST_MOCK_UDEV_LIST_ENTRY_H_
