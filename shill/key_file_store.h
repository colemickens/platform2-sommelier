// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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

  void set_path(const base::FilePath &path) { path_ = path; }
  const base::FilePath &path() const { return path_; }

  // Returns true if the store exists and is non-empty.
  bool IsNonEmpty() const;

  // Opens the store. Returns true on success. This method must be
  // invoked before using any of the getters or setters.
  // This method does not complete gracefully if invoked on a store
  // that has been opened already but not closed yet.
  bool Open();

  // Closes the store and flushes it to persistent storage. Returns true on
  // success. Note that the store is considered closed even if Close returns
  // false.
  // This method does not complete gracefully if invoked on a store
  // that has not been opened successfully or has been closed already.
  bool Close();

  // Mark the underlying file store as corrupted, moving the data file
  // to a new filename.  This will prevent the file from being re-opened
  // the next time Open() is called.
  bool MarkAsCorrupted();

  // Inherited from StoreInterface.
  virtual bool Flush() override;
  virtual std::set<std::string> GetGroups() const override;
  virtual std::set<std::string> GetGroupsWithKey(const std::string &key)
      const override;
  virtual std::set<std::string> GetGroupsWithProperties(
      const KeyValueStore &properties) const override;
  virtual bool ContainsGroup(const std::string &group) const override;
  virtual bool DeleteKey(const std::string &group, const std::string &key)
      override;
  virtual bool DeleteGroup(const std::string &group) override;
  virtual bool SetHeader(const std::string &header) override;
  virtual bool GetString(const std::string &group,
                         const std::string &key,
                         std::string *value) const override;
  virtual bool SetString(const std::string &group,
                         const std::string &key,
                         const std::string &value) override;
  virtual bool GetBool(const std::string &group,
                       const std::string &key,
                       bool *value) const override;
  virtual bool SetBool(const std::string &group,
                       const std::string &key,
                       bool value) override;
  virtual bool GetInt(const std::string &group,
                      const std::string &key,
                      int *value) const override;
  virtual bool SetInt(const std::string &group,
                      const std::string &key,
                      int value) override ;
  virtual bool GetUint64(const std::string &group,
                         const std::string &key,
                         uint64 *value) const override;
  virtual bool SetUint64(const std::string &group,
                         const std::string &key,
                         uint64 value) override;
  virtual bool GetStringList(const std::string &group,
                             const std::string &key,
                             std::vector<std::string> *value) const override;
  virtual bool SetStringList(const std::string &group,
                             const std::string &key,
                             const std::vector<std::string> &value) override;
  virtual bool GetCryptedString(const std::string &group,
                                const std::string &key,
                                std::string *value) override;
  virtual bool SetCryptedString(const std::string &group,
                                const std::string &key,
                                const std::string &value) override;
 private:
  FRIEND_TEST(KeyFileStoreTest, OpenClose);
  FRIEND_TEST(KeyFileStoreTest, OpenFail);

  static const char kCorruptSuffix[];

  void ReleaseKeyFile();
  bool DoesGroupMatchProperties(const std::string &group,
                                const KeyValueStore &properties) const;

  GLib *glib_;
  CryptoProvider crypto_;
  GKeyFile *key_file_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(KeyFileStore);
};

}  // namespace shill

#endif  // SHILL_KEY_FILE_STORE_
