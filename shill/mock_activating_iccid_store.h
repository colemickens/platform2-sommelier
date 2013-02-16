// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ACTIVATING_ICCID_STORE_H_
#define SHILL_MOCK_ACTIVATING_ICCID_STORE_H_

#include <string>

#include <base/file_path.h>
#include <gmock/gmock.h>

#include "shill/activating_iccid_store.h"

namespace shill {

class MockActivatingIccidStore : public ActivatingIccidStore {
 public:
  MockActivatingIccidStore();
  virtual ~MockActivatingIccidStore();

  MOCK_METHOD2(InitStorage, bool(GLib *glib, const FilePath &storage_path));
  MOCK_CONST_METHOD1(GetActivationState, State(const std::string &iccid));
  MOCK_METHOD2(SetActivationState,
               bool(const std::string &iccid, State state));
  MOCK_METHOD1(RemoveEntry, bool(const std::string &iccid));
};

}  // namespace shill

#endif  // SHILL_MOCK_ACTIVATING_ICCID_STORE_H_

