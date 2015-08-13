// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_JSON_STORE_H_
#define SHILL_JSON_STORE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <chromeos/variant_dictionary.h>

#include "shill/store_interface.h"

namespace shill {

class JsonStore : public StoreInterface {
 public:
  JsonStore();

  // TODO(quiche): Determine if we need a dtor. In particular, we'll
  // need one of StoreInterface implementations are expected to
  // automatically Flush() before destruction.

  // Inherited from StoreInterface.
  bool Flush() override;
  std::set<std::string> GetGroups() const override;
  std::set<std::string> GetGroupsWithKey(const std::string& key) const override;
  std::set<std::string> GetGroupsWithProperties(
      const KeyValueStore& properties) const override;
  bool ContainsGroup(const std::string& group) const override;
  bool DeleteKey(const std::string& group, const std::string& key) override;
  bool DeleteGroup(const std::string& group) override;
  bool SetHeader(const std::string& header) override;
  bool GetString(const std::string& group,
                 const std::string& key,
                 std::string* value) const override;
  bool SetString(const std::string& group,
                 const std::string& key,
                 const std::string& value) override;
  bool GetBool(const std::string& group,
               const std::string& key,
               bool* value) const override;
  bool SetBool(const std::string& group,
               const std::string& key,
               bool value) override;
  bool GetInt(const std::string& group,
              const std::string& key,
              int* value) const override;
  bool SetInt(const std::string& group,
              const std::string& key,
              int value) override;
  bool GetUint64(const std::string& group,
                 const std::string& key,
                 uint64_t* value) const override;
  bool SetUint64(const std::string& group,
                 const std::string& key,
                 uint64_t value) override;
  bool GetStringList(const std::string& group,
                     const std::string& key,
                     std::vector<std::string>* value) const override;
  bool SetStringList(const std::string& group,
                     const std::string& key,
                     const std::vector<std::string>& value) override;
  // GetCryptedString is non-const for legacy reasons. See
  // KeyFileStore::SetCryptedString() for details.
  bool GetCryptedString(const std::string& group,
                        const std::string& key,
                        std::string* value) override;
  bool SetCryptedString(const std::string& group,
                        const std::string& key,
                        const std::string& value) override;

 private:
  template<typename T> bool ReadSetting(
      const std::string& group, const std::string& key, T* out) const;
  template<typename T> bool WriteSetting(
      const std::string& group, const std::string& key, const T& new_value);

  std::map<std::string, chromeos::VariantDictionary> group_name_to_settings_;

  DISALLOW_COPY_AND_ASSIGN(JsonStore);
};

}  // namespace shill

#endif  // SHILL_JSON_STORE_H_
