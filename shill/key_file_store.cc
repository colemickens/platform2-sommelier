// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/key_file_store.h"

#include <base/file_util.h>
#include <base/logging.h>

using std::set;
using std::string;
using std::vector;

namespace shill {

KeyFileStore::KeyFileStore(GLib *glib)
    : glib_(glib),
      crypto_(glib),
      key_file_(NULL) {}

KeyFileStore::~KeyFileStore() {
  ReleaseKeyFile();
}

void KeyFileStore::ReleaseKeyFile() {
  if (key_file_) {
    glib_->KeyFileFree(key_file_);
    key_file_ = NULL;
  }
}

bool KeyFileStore::Open() {
  CHECK(!path_.empty());
  CHECK(!key_file_);
  crypto_.Init();
  key_file_ = glib_->KeyFileNew();
  int64 file_size = 0;
  if (!file_util::GetFileSize(path_, &file_size) || file_size == 0) {
    LOG(INFO) << "Creating a new key file at " << path_.value();
    return true;
  }
  GError *error = NULL;
  if (glib_->KeyFileLoadFromFile(
          key_file_,
          path_.value().c_str(),
          static_cast<GKeyFileFlags>(G_KEY_FILE_KEEP_COMMENTS |
                                     G_KEY_FILE_KEEP_TRANSLATIONS),
          &error)) {
    return true;
  }
  LOG(ERROR) << "Failed to load key file from " << path_.value() << ": "
             << glib_->ConvertErrorToMessage(error);
  ReleaseKeyFile();
  return false;
}

bool KeyFileStore::Close() {
  CHECK(key_file_);
  GError *error = NULL;
  gsize length = 0;
  gchar *data = glib_->KeyFileToData(key_file_, &length, &error);
  ReleaseKeyFile();

  bool success = true;
  if (path_.empty()) {
    LOG(ERROR) << "Empty key file path.";
    success = false;
  }
  if (success && (!data || error)) {
    LOG(ERROR) << "Failed to convert key file to string: "
               << glib_->ConvertErrorToMessage(error);
    success = false;
  }
  if (success && file_util::WriteFile(path_, data, length) != length) {
    LOG(ERROR) << "Failed to store key file: " << path_.value();
    success = false;
  }
  glib_->Free(data);
  return success;
}

set<string> KeyFileStore::GetGroups() {
  CHECK(key_file_);
  gsize length = 0;
  gchar **groups = g_key_file_get_groups(key_file_, &length);
  if (!groups) {
    LOG(ERROR) << "Unable to obtain groups.";
    return set<string>();
  }
  set<string> group_set(groups, groups + length);
  glib_->Strfreev(groups);
  return group_set;
}

bool KeyFileStore::ContainsGroup(const string &group) {
  CHECK(key_file_);
  return glib_->KeyFileHasGroup(key_file_, group.c_str());
}

bool KeyFileStore::DeleteKey(const string &group, const string &key) {
  CHECK(key_file_);
  GError *error = NULL;
  glib_->KeyFileRemoveKey(key_file_, group.c_str(), key.c_str(), &error);
  if (error) {
    LOG(ERROR) << "Failed to delete (" << group << ":" << key << "): "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  return true;
}

bool KeyFileStore::DeleteGroup(const string &group) {
  CHECK(key_file_);
  GError *error = NULL;
  glib_->KeyFileRemoveGroup(key_file_, group.c_str(), &error);
  if (error) {
    LOG(ERROR) << "Failed to delete group " << group << ": "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  return true;
}

bool KeyFileStore::GetString(const string &group,
                             const string &key,
                             string *value) {
  CHECK(key_file_);
  GError *error = NULL;
  gchar *data =
      glib_->KeyFileGetString(key_file_, group.c_str(), key.c_str(), &error);
  if (!data) {
    LOG(ERROR) << "Failed to lookup (" << group << ":" << key << "): "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  if (value) {
    *value = data;
  }
  glib_->Free(data);
  return true;
}

bool KeyFileStore::SetString(const string &group,
                             const string &key,
                             const string &value) {
  CHECK(key_file_);
  glib_->KeyFileSetString(key_file_, group.c_str(), key.c_str(), value.c_str());
  return true;
}

bool KeyFileStore::GetBool(const string &group,
                           const string &key,
                           bool *value) {
  CHECK(key_file_);
  GError *error = NULL;
  gboolean data =
      glib_->KeyFileGetBoolean(key_file_, group.c_str(), key.c_str(), &error);
  if (error) {
    LOG(ERROR) << "Failed to lookup (" << group << ":" << key << "): "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  if (value) {
    *value = data;
  }
  return true;
}

bool KeyFileStore::SetBool(const string &group, const string &key, bool value) {
  CHECK(key_file_);
  glib_->KeyFileSetBoolean(key_file_,
                           group.c_str(),
                           key.c_str(),
                           value ? TRUE : FALSE);
  return true;
}

bool KeyFileStore::GetInt(const string &group, const string &key, int *value) {
  CHECK(key_file_);
  GError *error = NULL;
  gint data =
      glib_->KeyFileGetInteger(key_file_, group.c_str(), key.c_str(), &error);
  if (error) {
    LOG(ERROR) << "Failed to lookup (" << group << ":" << key << "): "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  if (value) {
    *value = data;
  }
  return true;
}

bool KeyFileStore::SetInt(const string &group, const string &key, int value) {
  CHECK(key_file_);
  glib_->KeyFileSetInteger(key_file_, group.c_str(), key.c_str(), value);
  return true;
}

bool KeyFileStore::GetStringList(const string &group,
                                 const string &key,
                                 vector<string> *value) {
  CHECK(key_file_);
  gsize length = 0;
  GError *error = NULL;
  gchar **data = glib_->KeyFileGetStringList(key_file_,
                                             group.c_str(),
                                             key.c_str(),
                                             &length,
                                             &error);
  if (!data) {
    LOG(ERROR) << "Failed to lookup (" << group << ":" << key << "): "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  if (value) {
    value->assign(data, data + length);
  }
  glib_->Strfreev(data);
  return true;
}

bool KeyFileStore::SetStringList(const string &group,
                                 const string &key,
                                 const vector<string> &value) {
  CHECK(key_file_);
  vector<const char *> list;
  for (vector<string>::const_iterator it = value.begin();
       it != value.end(); ++it) {
    list.push_back(it->c_str());
  }
  glib_->KeyFileSetStringList(key_file_,
                              group.c_str(),
                              key.c_str(),
                              list.data(),
                              list.size());
  return true;
}

bool KeyFileStore::GetCryptedString(const string &group,
                                    const string &key,
                                    string *value) {
  if (!GetString(group, key, value)) {
    return false;
  }
  if (value) {
    *value = crypto_.Decrypt(*value);
  }
  return true;
}

bool KeyFileStore::SetCryptedString(const string &group,
                                    const string &key,
                                    const string &value) {
  return SetString(group, key, crypto_.Encrypt(value));
}

}  // namespace shill
