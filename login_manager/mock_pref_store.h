// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_PREF_STORE_H_
#define LOGIN_MANAGER_MOCK_PREF_STORE_H_

#include "login_manager/pref_store.h"

#include <unistd.h>
#include <gmock/gmock.h>

namespace login_manager {
class MockPrefStore : public PrefStore {
 public:
  MockPrefStore() : PrefStore(FilePath("")) {}
  virtual ~MockPrefStore() {}
  MOCK_METHOD0(LoadOrCreate, bool(void));
  MOCK_METHOD0(Persist, bool(void));
  MOCK_METHOD2(Whitelist, void(const std::string&, const std::string&));
  MOCK_METHOD1(Unwhitelist, void(const std::string&));
  MOCK_METHOD2(GetFromWhitelist, bool(const std::string&, std::string*));
  MOCK_METHOD1(EnumerateWhitelisted, void(std::vector<std::string>*));
  MOCK_METHOD3(Set, void(const std::string&,
                         const std::string&,
                         const std::string&));
  MOCK_METHOD3(Get, bool(const std::string&, std::string*, std::string*));
  MOCK_METHOD3(Remove, bool(const std::string&, std::string*, std::string*));
  MOCK_METHOD1(Delete, void(const std::string&));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_PREF_STORE_H_
