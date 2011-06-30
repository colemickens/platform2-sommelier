// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_KEY_FILE_STORE_
#define SHILL_KEY_FILE_STORE_

#include <base/file_path.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/crypto_provider.h"
#include "shill/glib.h"
#include "shill/store_interface.h"

namespace shill {

// A key file store implementation of the store interface. See
// http://www.gtk.org/api/2.6/glib/glib-Key-value-file-parser.html for details
// of the key file format.
class KeyFileStore : public StoreInterface {
 public:
  explicit KeyFileStore(GLib *glib);
  virtual ~KeyFileStore();

  void set_path(const FilePath &path) { path_ = path; }
  const FilePath &path() const { return path_; }

  // Inherited from StoreInterface.
  virtual bool Open();
  virtual bool Close();
  virtual std::set<std::string> GetGroups();
  virtual bool ContainsGroup(const std::string &group);
  virtual bool DeleteKey(const std::string &group, const std::string &key);
  virtual bool DeleteGroup(const std::string &group);
  virtual bool GetString(const std::string &group,
                         const std::string &key,
                         std::string *value);
  virtual bool SetString(const std::string &group,
                         const std::string &key,
                         const std::string &value);
  virtual bool GetBool(const std::string &group,
                       const std::string &key,
                       bool *value);
  virtual bool SetBool(const std::string &group,
                       const std::string &key,
                       bool value);
  virtual bool GetInt(const std::string &group,
                      const std::string &key,
                      int *value);
  virtual bool SetInt(const std::string &group,
                      const std::string &key,
                      int value);
  virtual bool GetStringList(const std::string &group,
                             const std::string &key,
                             std::vector<std::string> *value);
  virtual bool SetStringList(const std::string &group,
                             const std::string &key,
                             const std::vector<std::string> &value);
  virtual bool GetCryptedString(const std::string &group,
                                const std::string &key,
                                std::string *value);
  virtual bool SetCryptedString(const std::string &group,
                                const std::string &key,
                                const std::string &value);
 private:
  FRIEND_TEST(KeyFileStoreTest, OpenClose);
  FRIEND_TEST(KeyFileStoreTest, OpenFail);

  void ReleaseKeyFile();

  GLib *glib_;
  CryptoProvider crypto_;
  GKeyFile *key_file_;
  FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(KeyFileStore);
};

}  // namespace shill

#endif  // SHILL_KEY_FILE_STORE_
