// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_STORE_INTERFACE_
#define SHILL_STORE_INTERFACE_

#include <set>
#include <string>
#include <vector>

namespace shill {

// An interface to a persistent store implementation.
class StoreInterface {
 public:
  virtual ~StoreInterface() {}

  // Flush current in-memory data to disk.
  virtual bool Flush() = 0;

  // Returns a set of all groups contained in the store.
  virtual std::set<std::string> GetGroups() = 0;

  // Returns true if the store contains |group|, false otherwise.
  virtual bool ContainsGroup(const std::string &group) = 0;

  // Deletes |group|:|key|. Returns true on success.
  virtual bool DeleteKey(const std::string &group, const std::string &key) = 0;

  // Deletes |group|. Returns true on success.
  virtual bool DeleteGroup(const std::string &group) = 0;

  // Sets a descriptive header on the key file.
  virtual bool SetHeader(const std::string &header) = 0;

  // Gets a string |value| associated with |group|:|key|. Returns true on
  // success and false on failure (including when |group|:|key| is not present
  // in the store).
  virtual bool GetString(const std::string &group,
                         const std::string &key,
                         std::string *value) = 0;

  // Associates |group|:|key| with a string |value|. Returns true on success,
  // false otherwise.
  virtual bool SetString(const std::string &group,
                         const std::string &key,
                         const std::string &value) = 0;

  // Gets a boolean |value| associated with |group|:|key|. Returns true on
  // success and false on failure (including when the |group|:|key| is not
  // present in the store).
  virtual bool GetBool(const std::string &group,
                       const std::string &key,
                       bool *value) = 0;

  // Associates |group|:|key| with a boolean |value|. Returns true on success,
  // false otherwise.
  virtual bool SetBool(const std::string &group,
                       const std::string &key,
                       bool value) = 0;

  // Gets a integer |value| associated with |group|:|key|. Returns true on
  // success and false on failure (including when the |group|:|key| is not
  // present in the store).
  virtual bool GetInt(const std::string &group,
                      const std::string &key,
                      int *value) = 0;

  // Associates |group|:|key| with an integer |value|. Returns true on success,
  // false otherwise.
  virtual bool SetInt(const std::string &group,
                      const std::string &key,
                      int value) = 0;

  // Gets a string list |value| associated with |group|:|key|. Returns true on
  // success and false on failure (including when |group|:|key| is not present
  // in the store).
  virtual bool GetStringList(const std::string &group,
                             const std::string &key,
                             std::vector<std::string> *value) = 0;

  // Associates |group|:|key| with a string list |value|. Returns true on
  // success, false otherwise.
  virtual bool SetStringList(const std::string &group,
                             const std::string &key,
                             const std::vector<std::string> &value) = 0;

  // Gets and decrypts string |value| associated with |group|:|key|. Returns
  // true on success and false on failure (including when |group|:|key| is not
  // present in the store).
  virtual bool GetCryptedString(const std::string &group,
                                const std::string &key,
                                std::string *value) = 0;

  // Associates |group|:|key| with a string |value| after encrypting it. Returns
  // true on success, false otherwise.
  virtual bool SetCryptedString(const std::string &group,
                                const std::string &key,
                                const std::string &value) = 0;
};

}  // namespace shill

#endif  // SHILL_STORE_INTERFACE_
