// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_KEY_VALUE_STORE_H_
#define SHILL_KEY_VALUE_STORE_H_

#include <map>
#include <string>
#include <vector>

namespace shill {

class KeyValueStore {
  // A simple store for key-value pairs, which supports (a limited set of)
  // heterogenous value types.
  //
  // Compare to PropertyStore, which enables a class to (selectively)
  // expose its instance members as properties accessible via
  // RPC. (RPC support for ProperyStore is implemented in a
  // protocol-specific adaptor. e.g. dbus_adpator.)
  //
  // Implemented separately from PropertyStore, to avoid complicating
  // the PropertyStore interface. In particular, objects implementing the
  // PropertyStore interface always provide the storage themselves. In
  // contrast, users of KeyValueStore expect KeyValueStore to provide
  // storage.
 public:
  KeyValueStore();

  void Clear();
  void CopyFrom(const KeyValueStore& b);
  bool Equals(const KeyValueStore& other) const;
  bool IsEmpty();

  bool ContainsBool(const std::string& name) const;
  bool ContainsByteArrays(const std::string& name) const;
  bool ContainsInt(const std::string& name) const;
  bool ContainsInt16(const std::string& name) const;
  bool ContainsKeyValueStore(const std::string& name) const;
  bool ContainsRpcIdentifier(const std::string& name) const;
  bool ContainsString(const std::string& name) const;
  bool ContainsStringmap(const std::string& name) const;
  bool ContainsStrings(const std::string& name) const;
  bool ContainsUint(const std::string& name) const;
  bool ContainsUint16(const std::string& name) const;
  bool ContainsUint8s(const std::string& name) const;
  bool ContainsUint32s(const std::string& name) const;

  bool GetBool(const std::string& name) const;
  const std::vector<std::vector<uint8_t>>& GetByteArrays(
      const std::string& name) const;
  int32_t GetInt(const std::string& name) const;
  int16_t GetInt16(const std::string& name) const;
  const KeyValueStore& GetKeyValueStore(const std::string& name) const;
  const std::string& GetRpcIdentifier(const std::string& name) const;
  const std::string& GetString(const std::string& name) const;
  const std::map<std::string, std::string>& GetStringmap(
      const std::string& name) const;
  const std::vector<std::string>& GetStrings(const std::string& name) const;
  uint32_t GetUint(const std::string& name) const;
  uint16_t GetUint16(const std::string& name) const;
  const std::vector<uint8_t>& GetUint8s(const std::string& name) const;
  const std::vector<uint32_t>& GetUint32s(const std::string& name) const;

  void SetBool(const std::string& name, bool value);
  void SetByteArrays(const std::string& name,
                     const std::vector<std::vector<uint8_t>>& value);
  void SetInt(const std::string& name, int32_t value);
  void SetInt16(const std::string& name, int16_t value);
  void SetKeyValueStore(const std::string& name, const KeyValueStore& value);
  void SetRpcIdentifier(const std::string& name, const std::string& value);
  void SetString(const std::string& name, const std::string& value);
  void SetStringmap(const std::string& name,
                    const std::map<std::string, std::string>& value);
  void SetStrings(const std::string& name,
                  const std::vector<std::string>& value);
  void SetUint(const std::string& name, uint32_t value);
  void SetUint16(const std::string& name, uint16_t value);
  void SetUint8s(const std::string& name, const std::vector<uint8_t>& value);
  void SetUint32s(const std::string& name, const std::vector<uint32_t>& value);

  void RemoveString(const std::string& name);
  void RemoveStringmap(const std::string& name);
  void RemoveStrings(const std::string& name);
  void RemoveInt(const std::string& name);
  void RemoveKeyValueStore(const std::string& name);
  void RemoveInt16(const std::string& name);
  void RemoveRpcIdentifier(const std::string& name);
  void RemoveByteArrays(const std::string& name);
  void RemoveUint16(const std::string& name);
  void RemoveUint8s(const std::string& name);
  void RemoveUint32s(const std::string& name);

  // If |name| is in this store returns its value, otherwise returns
  // |default_value|.
  bool LookupBool(const std::string& name, bool default_value) const;
  int LookupInt(const std::string& name, int default_value) const;
  std::string LookupString(const std::string& name,
                           const std::string& default_value) const;

  const std::map<std::string, bool>& bool_properties() const {
    return bool_properties_;
  }
  const std::map<std::string, std::vector<std::vector<uint8_t>>>&
      byte_arrays_properties() const {
    return byte_arrays_properties_;
  }

  const std::map<std::string, int32_t>& int_properties() const {
    return int_properties_;
  }
  const std::map<std::string, int16_t>& int16_properties() const {
    return int16_properties_;
  }
  const std::map<std::string, KeyValueStore>
      &key_value_store_properties() const {
    return key_value_store_properties_;
  }
  const std::map<std::string, std::string>& rpc_identifier_properties() const {
    return rpc_identifier_properties_;
  }
  const std::map<std::string, std::string>& string_properties() const {
    return string_properties_;
  }
  const std::map<
      std::string,
      std::map<std::string, std::string>>& stringmap_properties() const {
    return stringmap_properties_;
  }
  const std::map<std::string,
                 std::vector<std::string>>& strings_properties() const {
    return strings_properties_;
  }
  const std::map<std::string, uint32_t>& uint_properties() const {
    return uint_properties_;
  }
  const std::map<std::string, uint16_t>& uint16_properties() const {
    return uint16_properties_;
  }
  const std::map<std::string, std::vector<uint8_t>>& uint8s_properties() const {
    return uint8s_properties_;
  }
  const std::map<std::string, std::vector<uint32_t>>&
      uint32s_properties() const {
    return uint32s_properties_;
  }

 private:
  // Recursively compare KeyValueStore properties with |other|.
  bool KeyValueStorePropertiesAreEqual(const KeyValueStore& other) const;

  std::map<std::string, bool> bool_properties_;
  std::map<std::string, std::vector<std::vector<uint8_t>>>
      byte_arrays_properties_;
  std::map<std::string, int32_t> int_properties_;
  std::map<std::string, int16_t> int16_properties_;
  std::map<std::string, KeyValueStore> key_value_store_properties_;
  std::map<std::string, std::string> rpc_identifier_properties_;
  std::map<std::string, std::string> string_properties_;
  std::map<std::string,
           std::map<std::string, std::string>> stringmap_properties_;
  std::map<std::string, std::vector<std::string>> strings_properties_;
  std::map<std::string, uint32_t> uint_properties_;
  std::map<std::string, uint16_t> uint16_properties_;
  std::map<std::string, std::vector<uint8_t>> uint8s_properties_;
  std::map<std::string, std::vector<uint32_t>> uint32s_properties_;
};

}  // namespace shill

#endif  // SHILL_KEY_VALUE_STORE_H_
