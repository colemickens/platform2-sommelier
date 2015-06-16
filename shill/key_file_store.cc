// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/key_file_store.h"

#include <map>

#include <base/files/important_file_writer.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/scoped_umask.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kStorage;
static string ObjectID(const KeyFileStore* k) { return "(key_file_store)"; }
}

const char KeyFileStore::kCorruptSuffix[] = ".corrupted";

KeyFileStore::KeyFileStore(GLib* glib)
    : glib_(glib),
      crypto_(glib),
      key_file_(nullptr) {}

KeyFileStore::~KeyFileStore() {
  ReleaseKeyFile();
}

void KeyFileStore::ReleaseKeyFile() {
  if (key_file_) {
    glib_->KeyFileFree(key_file_);
    key_file_ = nullptr;
  }
}

bool KeyFileStore::IsNonEmpty() const {
  int64_t file_size = 0;
  return base::GetFileSize(path_, &file_size) && file_size != 0;
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
  GError* error = nullptr;
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
  GError* error = nullptr;
  gsize length = 0;
  gchar* data = glib_->KeyFileToData(key_file_, &length, &error);

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
    ScopedUmask owner_only_umask(~(S_IRUSR | S_IWUSR));
    success = base::ImportantFileWriter::WriteFileAtomically(path_, data);
    if (!success) {
      LOG(ERROR) << "Failed to store key file: " << path_.value();
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
  gchar** groups = glib_->KeyFileGetGroups(key_file_, &length);
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
set<string> KeyFileStore::GetGroupsWithKey(const string& key) const {
  set<string> groups = GetGroups();
  set<string> groups_with_key;
  for (const auto& group : groups) {
    if (glib_->KeyFileHasKey(key_file_, group.c_str(), key.c_str(), nullptr)) {
      groups_with_key.insert(group);
    }
  }
  return groups_with_key;
}

set<string> KeyFileStore::GetGroupsWithProperties(
     const KeyValueStore& properties) const {
  set<string> groups = GetGroups();
  set<string> groups_with_properties;
  for (const auto& group : groups) {
    if (DoesGroupMatchProperties(group, properties)) {
      groups_with_properties.insert(group);
    }
  }
  return groups_with_properties;
}

bool KeyFileStore::ContainsGroup(const string& group) const {
  CHECK(key_file_);
  return glib_->KeyFileHasGroup(key_file_, group.c_str());
}

bool KeyFileStore::DeleteKey(const string& group, const string& key) {
  CHECK(key_file_);
  GError* error = nullptr;
  glib_->KeyFileRemoveKey(key_file_, group.c_str(), key.c_str(), &error);
  if (error && error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete (" << group << ":" << key << "): "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  return true;
}

bool KeyFileStore::DeleteGroup(const string& group) {
  CHECK(key_file_);
  GError* error = nullptr;
  glib_->KeyFileRemoveGroup(key_file_, group.c_str(), &error);
  if (error && error->code != G_KEY_FILE_ERROR_GROUP_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete group " << group << ": "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  return true;
}

bool KeyFileStore::SetHeader(const string& header) {
  GError* error = nullptr;
  glib_->KeyFileSetComment(key_file_, nullptr, nullptr, header.c_str(), &error);
  if (error) {
    LOG(ERROR) << "Failed to to set header: "
               << glib_->ConvertErrorToMessage(error);
    return false;
  }
  return true;
}

bool KeyFileStore::GetString(const string& group,
                             const string& key,
                             string* value) const {
  CHECK(key_file_);
  GError* error = nullptr;
  gchar* data =
      glib_->KeyFileGetString(key_file_, group.c_str(), key.c_str(), &error);
  if (!data) {
    string s = glib_->ConvertErrorToMessage(error);
    SLOG(this, 10) << "Failed to lookup (" << group << ":" << key << "): " << s;
    return false;
  }
  if (value) {
    *value = data;
  }
  glib_->Free(data);
  return true;
}

bool KeyFileStore::SetString(const string& group,
                             const string& key,
                             const string& value) {
  CHECK(key_file_);
  glib_->KeyFileSetString(key_file_, group.c_str(), key.c_str(), value.c_str());
  return true;
}

bool KeyFileStore::GetBool(const string& group,
                           const string& key,
                           bool* value) const {
  CHECK(key_file_);
  GError* error = nullptr;
  gboolean data =
      glib_->KeyFileGetBoolean(key_file_, group.c_str(), key.c_str(), &error);
  if (error) {
    string s = glib_->ConvertErrorToMessage(error);
    SLOG(this, 10) << "Failed to lookup (" << group << ":" << key << "): " << s;
    return false;
  }
  if (value) {
    *value = data;
  }
  return true;
}

bool KeyFileStore::SetBool(const string& group, const string& key, bool value) {
  CHECK(key_file_);
  glib_->KeyFileSetBoolean(key_file_,
                           group.c_str(),
                           key.c_str(),
                           value ? TRUE : FALSE);
  return true;
}

bool KeyFileStore::GetInt(
    const string& group, const string& key, int* value) const {
  CHECK(key_file_);
  GError* error = nullptr;
  gint data =
      glib_->KeyFileGetInteger(key_file_, group.c_str(), key.c_str(), &error);
  if (error) {
    string s = glib_->ConvertErrorToMessage(error);
    SLOG(this, 10) << "Failed to lookup (" << group << ":" << key << "): " << s;
    return false;
  }
  if (value) {
    *value = data;
  }
  return true;
}

bool KeyFileStore::SetInt(const string& group, const string& key, int value) {
  CHECK(key_file_);
  glib_->KeyFileSetInteger(key_file_, group.c_str(), key.c_str(), value);
  return true;
}

bool KeyFileStore::GetUint64(
    const string& group, const string& key, uint64_t* value) const {
  // Read the value in as a string and then convert to uint64_t because glib's
  // g_key_file_set_uint64 appears not to work correctly on 32-bit platforms
  // in unit tests.
  string data_string;
  if (!GetString(group, key, &data_string)) {
    return false;
  }

  uint64_t data;
  if (!base::StringToUint64(data_string, &data)) {
    SLOG(this, 10) << "Failed to convert (" << group << ":" << key << "): "
                   << "string to uint64_t conversion failed";
    return false;
  }

  if (value) {
    *value = data;
  }

  return true;
}

bool KeyFileStore::SetUint64(
    const string& group, const string& key, uint64_t value) {
  // Convert the value to a string first, then save the value because glib's
  // g_key_file_get_uint64 appears not to work on 32-bit platforms in our
  // unit tests.
  return SetString(group, key, base::Uint64ToString(value));
}

bool KeyFileStore::GetStringList(const string& group,
                                 const string& key,
                                 vector<string>* value) const {
  CHECK(key_file_);
  gsize length = 0;
  GError* error = nullptr;
  gchar** data = glib_->KeyFileGetStringList(key_file_,
                                             group.c_str(),
                                             key.c_str(),
                                             &length,
                                             &error);
  if (!data) {
    string s = glib_->ConvertErrorToMessage(error);
    SLOG(this, 10) << "Failed to lookup (" << group << ":" << key << "): " << s;
    return false;
  }
  if (value) {
    value->assign(data, data + length);
  }
  glib_->Strfreev(data);
  return true;
}

bool KeyFileStore::SetStringList(const string& group,
                                 const string& key,
                                 const vector<string>& value) {
  CHECK(key_file_);
  vector<const char*> list;
  for (const auto& string_entry : value) {
    list.push_back(string_entry.c_str());
  }
  glib_->KeyFileSetStringList(key_file_,
                              group.c_str(),
                              key.c_str(),
                              list.data(),
                              list.size());
  return true;
}

bool KeyFileStore::GetCryptedString(const string& group,
                                    const string& key,
                                    string* value) {
  if (!GetString(group, key, value)) {
    return false;
  }
  if (value) {
    *value = crypto_.Decrypt(*value);
  }
  return true;
}

bool KeyFileStore::SetCryptedString(const string& group,
                                    const string& key,
                                    const string& value) {
  return SetString(group, key, crypto_.Encrypt(value));
}

bool KeyFileStore::DoesGroupMatchProperties(
    const string& group, const KeyValueStore& properties) const {
  map<string, bool>::const_iterator bool_it;
  for (const auto& property : properties.bool_properties()) {
    bool value;
    if (!GetBool(group, property.first, &value) || value != property.second) {
      return false;
    }
  }
  for (const auto& property : properties.int_properties()) {
    int value;
    if (!GetInt(group, property.first, &value) || value != property.second) {
      return false;
    }
  }
  for (const auto& property : properties.string_properties()) {
    string value;
    if (!GetString(group, property.first, &value) || value != property.second) {
      return false;
    }
  }
  return true;
}

}  // namespace shill
