// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_STUB_STORAGE_H_
#define SHILL_STUB_STORAGE_H_

#include <set>
#include <string>
#include <vector>

#include "shill/store_interface.h"

namespace shill {

// A stub implementation of StoreInterface.
class StubStorage : public StoreInterface {
 public:
  virtual ~StubStorage() {}

  virtual bool Flush() override { return false; }
  virtual std::set<std::string> GetGroups() const override {
    return {};
  }
  virtual std::set<std::string> GetGroupsWithKey(
      const std::string &key) const override {
    return {};
  }
  virtual std::set<std::string> GetGroupsWithProperties(
      const KeyValueStore &properties) const override {
    return {};
  }
  virtual bool ContainsGroup(const std::string &group) const override {
    return false;
  }
  virtual bool DeleteKey(const std::string &group, const std::string &key)
      override { return false; }
  virtual bool DeleteGroup(const std::string &group) override { return false; }
  virtual bool SetHeader(const std::string &header) override { return false; }
  virtual bool GetString(const std::string &group,
                         const std::string &key,
                         std::string *value) const override {
    return false;
  }
  virtual bool SetString(const std::string &group,
                         const std::string &key,
                         const std::string &value) override {
    return false;
  }
  virtual bool GetBool(const std::string &group,
                       const std::string &key,
                       bool *value) const override {
    return false;
  }
  virtual bool SetBool(const std::string &group,
                       const std::string &key,
                       bool value) override {
    return false;
  }
  virtual bool GetInt(const std::string &group,
                      const std::string &key,
                      int *value) const override {
    return false;
  }
  virtual bool SetInt(const std::string &group,
                      const std::string &key,
                      int value) override {
    return false;
  }
  virtual bool GetUint64(const std::string &group,
                         const std::string &key,
                         uint64_t *value) const override {
    return false;
  }
  virtual bool SetUint64(const std::string &group,
                         const std::string &key,
                         uint64_t value) override {
    return false;
  }
  virtual bool GetStringList(const std::string &group,
                             const std::string &key,
                             std::vector<std::string> *value) const override {
    return false;
  }
  virtual bool SetStringList(const std::string &group,
                             const std::string &key,
                             const std::vector<std::string> &value) override {
    return false;
  }
  virtual bool GetCryptedString(const std::string &group,
                                const std::string &key,
                                std::string *value) override {
    return false;
  }
  virtual bool SetCryptedString(const std::string &group,
                                const std::string &key,
                                const std::string &value) override {
    return false;
  }
};

}  // namespace shill

#endif  // SHILL_STUB_STORAGE_H_
