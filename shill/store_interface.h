// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_STORE_INTERFACE_
#define SHILL_STORE_INTERFACE_

#include <set>
#include <string>

namespace shill {

// An interface to a persistent store implementation.
class StoreInterface {
 public:
  virtual ~StoreInterface() {}

  // Opens the store. Returns true on success. This method must be invoked
  // before using any of the getters or setters. This method is not required
  // complete gracefully if invoked on a store that has been opened already but
  // not closed yet.
  virtual bool Open() = 0;

  // Closes the store and flushes it to persistent storage. Returns true on
  // success. Note that the store is considered closed even if Close returns
  // false. This method is not required to complete gracefully if invoked on a
  // store that has not been opened successfully or has been closed already.
  virtual bool Close() = 0;

  // Returns a set of all groups contained in the store.
  virtual std::set<std::string> GetGroups() = 0;

  // Returns true if the store contains |group|, false otherwise.
  virtual bool ContainsGroup(const std::string &group) = 0;

  // Deletes |group|:|key|. Returns true on success.
  virtual bool DeleteKey(const std::string &group, const std::string &key) = 0;

  // Deletes |group|. Returns true on success.
  virtual bool DeleteGroup(const std::string &group) = 0;

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
};

}  // namespace shill

#endif  // SHILL_STORE_INTERFACE_
