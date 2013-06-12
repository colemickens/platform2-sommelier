// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/key_file_store.h"

#include <map>

#include <base/file_util.h>
#include <base/string_number_conversions.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "shill/key_value_store.h"
#include "shill/logging.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {

const char KeyFileStore::kCorruptSuffix[] = ".corrupted";

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

bool KeyFileStore::IsNonEmpty() const {
  int64 file_size = 0;
  return file_util::GetFileSize(path_, &file_size) && file_size != 0;
}

bool KeyFileStore::Open() {
  CHECK(!path_.empty());
  CHECK(!key_file_);
  crypto_.Init();
  key_file_ = glib_->KeyFileNew();
  if (!IsNonEmpty()) {
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
  bool success = Flush();
  ReleaseKeyFile();
  return success;
}

bool KeyFileStore::Flush() {
  CHECK(key_file_);
  GError *error = NULL;
  gsize length = 0;
  gchar *data = glib_->KeyFileToData(key_file_, &length, &error);

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
  if (success) {
    // The profile file must be accessible by the owner only.
    int fd = creat(path_.value().c_str(), S_IRUSR | S_IWUSR);
    if (fd < 0) {
      LOG(ERROR) << "Failed to create key file " << path_.value();
      success = false;
    } else {
      int written = file_util::WriteFileDescriptor(fd, data, length);
      if (written < 0 || (static_cast<gsize>(written) != length)) {
        LOG(ERROR) << "Failed to store key file: " << path_.value();
        success = false;
      }
      // Call fsync() on this fd so that this file is completely written out,
      // since it is pivotal to our connectivity that these files remain
      // intact.
      fsync(fd);
      close(fd);
    }
  }
  glib_->Free(data);
  return success;
}

bool KeyFileStore::MarkAsCorrupted() {
  LOG(INFO) << "In " << __func__ << " for " << path_.value();
  if (path_.empty()) {
    LOG(ERROR) << "Empty key file path.";
    return false;
  }
  string corrupted_path = path_.value() + kCorruptSuffix;
  int ret =  rename(path_.value().c_str(), corrupted_path.c_str());
  if (ret != 0) {
    PLOG(ERROR) << "File rename failed";
    return false;
  }
  return true;
}

set<string> KeyFileStore::GetGroups() const {
  CHECK(key_file_);
  gsize length = 0;
  gchar **groups = glib_->KeyFileGetGroups(key_file_, &length);
  if (!groups) {
    LOG(ERROR) << "Unable to obtain groups.";
    return set<string>();
  }
  set<string> group_set(groups, groups + length);
  glib_->Strfreev(groups);
  return group_set;
}

// Returns a set so that caller can easily test whether a particular group
// is contained within this collection.
set<string> KeyFileStore::GetGroupsWithKey(const string &key) const {
  set<string> groups = GetGroups();
  set<string> groups_with_key;
  for (set<string>::iterator it = groups.begin(); it != groups.end(); ++it) {
    if (glib_->KeyFileHasKey(key_file_, (*it).c_str(), key.c_str(), NULL)) {
      groups_with_key.insert(*it);
    }
  }
  return groups_with_key;
}

set<string> KeyFileStore::GetGroupsWithProperties(
     const KeyValueStore &properties) const {
  set<string> groups = GetGroups();
  set<string> groups_with_properties;
  for (set<string>::iterator it = groups.begin(); it != groups.end(); ++it) {
    if (DoesGroupMatchProperties(*it, properties)) {
      groups_with_properties.insert(*it);
    }
  }
  return groups_with_properties;
}

bool KeyFileStore::ContainsGroup(const string &group) const {
  CHECK(key_file_);
  return glib_->KeyFileHasGroup(key_file_, group.c_str());
}

bool KeyFileStore::DeleteKey(const string &group, const string &key) {
  CHECK(key_file_);
  GError *error = NULL;
  glib_->KeyFileRemoveKey(key_file_, group.c_str(), key.c_str(), &error);
  if (error && error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
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
  if (error && error->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete group " << group << ": "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  return true;
}

bool KeyFileStore::SetHeader(const string &header) {
  GError *error = NULL;
  glib_->KeyFileSetComment(key_file_, NULL, NULL, header.c_str(), &error);
  if (error) {
    LOG(ERROR) << "Failed to to set header: "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  return true;
}

bool KeyFileStore::GetString(const string &group,
                             const string &key,
                             string *value) const {
  CHECK(key_file_);
  GError *error = NULL;
  gchar *data =
      glib_->KeyFileGetString(key_file_, group.c_str(), key.c_str(), &error);
  if (!data) {
    string s = glib_->ConvertErrorToMessage(error);
    SLOG(Storage, 10) << "Failed to lookup (" << group << ":" << key << "): "
                      << s;
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
                           bool *value) const {
  CHECK(key_file_);
  GError *error = NULL;
  gboolean data =
      glib_->KeyFileGetBoolean(key_file_, group.c_str(), key.c_str(), &error);
  if (error) {
    string s = glib_->ConvertErrorToMessage(error);
    SLOG(Storage, 10) << "Failed to lookup (" << group << ":" << key << "): "
                      << s;
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

bool KeyFileStore::GetInt(
    const string &group, const string &key, int *value) const {
  CHECK(key_file_);
  GError *error = NULL;
  gint data =
      glib_->KeyFileGetInteger(key_file_, group.c_str(), key.c_str(), &error);
  if (error) {
    string s = glib_->ConvertErrorToMessage(error);
    SLOG(Storage, 10) << "Failed to lookup (" << group << ":" << key << "): "
                      << s;
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

bool KeyFileStore::GetUint64(
    const string &group, const string &key, uint64 *value) const {
  // Read the value in as a string and then convert to uint64 because glib's
  // g_key_file_set_uint64 appears not to work correctly on 32-bit platforms
  // in unit tests.
  string data_string;
  if (!GetString(group, key, &data_string)) {
    return false;
  }

  uint64 data;
  if (!base::StringToUint64(data_string, &data)) {
    SLOG(Storage, 10) << "Failed to convert (" << group << ":" << key << "): "
                      << "string to uint64 conversion failed";
    return false;
  }

  if (value) {
    *value = data;
  }

  return true;
}

bool KeyFileStore::SetUint64(
    const string &group, const string &key, uint64 value) {
  // Convert the value to a string first, then save the value because glib's
  // g_key_file_get_uint64 appears not to work on 32-bit platforms in our
  // unit tests.
  return SetString(group, key, base::Uint64ToString(value));
}

bool KeyFileStore::GetStringList(const string &group,
                                 const string &key,
                                 vector<string> *value) const {
  CHECK(key_file_);
  gsize length = 0;
  GError *error = NULL;
  gchar **data = glib_->KeyFileGetStringList(key_file_,
                                             group.c_str(),
                                             key.c_str(),
                                             &length,
                                             &error);
  if (!data) {
    string s = glib_->ConvertErrorToMessage(error);
    SLOG(Storage, 10) << "Failed to lookup (" << group << ":" << key << "): "
                      << s;
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

bool KeyFileStore::DoesGroupMatchProperties(
    const string &group, const KeyValueStore &properties) const {
  map<string, bool>::const_iterator bool_it;
  for (bool_it = properties.bool_properties().begin();
       bool_it != properties.bool_properties().end();
       ++bool_it) {
    bool value;
    if (!GetBool(group, bool_it->first, &value) ||
        value != bool_it->second) {
      return false;
    }
  }
  map<string, int32>::const_iterator int_it;
  for (int_it = properties.int_properties().begin();
       int_it != properties.int_properties().end();
       ++int_it) {
    int value;
    if (!GetInt(group, int_it->first, &value) ||
        value != int_it->second) {
      return false;
    }
  }
  map<string, string>::const_iterator string_it;
  for (string_it = properties.string_properties().begin();
       string_it != properties.string_properties().end();
       ++string_it) {
    string value;
    if (!GetString(group, string_it->first, &value) ||
        value != string_it->second) {
      return false;
    }
  }
  return true;
}

}  // namespace shill
