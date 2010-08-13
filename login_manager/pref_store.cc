// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/pref_store.h"

#include "base/json/json_writer.h"
#include "base/file_util.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/values.h"

namespace login_manager {

// static
const char PrefStore::kWhitelistPrefix[] = "whitelist.";
// static
const char PrefStore::kValueField[] = ".value";
// static
const char PrefStore::kSignatureField[] = ".signature";
// static
const char PrefStore::kDefaultPath[] = "/var/lib/whitelist/preferences";

PrefStore::PrefStore(const FilePath& prefs_path)
    : prefs_(NULL),
      prefs_path_(prefs_path) {
}

PrefStore::~PrefStore() {}

bool PrefStore::LoadOrCreate() {
  // This will get blown away if we get a valid dictionary.
  prefs_.reset(new DictionaryValue);
  if (!file_util::PathExists(prefs_path_)) {
    return true;
  }
  std::string json;
  if (!file_util::ReadFileToString(prefs_path_, &json)) {
    PLOG(ERROR) << "Could not read prefs off disk: ";
    return false;
  }

  int error_code = 0;
  std::string error;
  // I take ownership of |new_dict|.
  scoped_ptr<Value> new_dict(base::JSONReader::ReadAndReturnError(json,
                                                                  false,
                                                                  &error_code,
                                                                  &error));
  if (!new_dict.get()) {
    LOG(ERROR) << "Could not parse prefs off disk: " << error;
    return false;
  }
  if (!new_dict->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Prefs file wasn't a dictionary!";
    return false;
  }
  prefs_.reset(static_cast<DictionaryValue*>(new_dict.release()));
  return true;
}

bool PrefStore::Persist() {
  std::string json;
  base::JSONWriter::Write(prefs_.get(),
                          true /* pretty print, for now */,
                          &json);
  int data_written = file_util::WriteFile(prefs_path_,
                                          json.c_str(),
                                          json.length());
  return (data_written == json.length());
}

void PrefStore::Whitelist(const std::string& name,
                          const std::string& signature) {
  std::string key(kWhitelistPrefix);
  key.append(name);
  prefs_->SetString(key, signature);
}

void PrefStore::Unwhitelist(const std::string& name) {
  std::string key(kWhitelistPrefix);
  key.append(name);
  prefs_->Remove(key, NULL);  // Passing NULL causes the value to get nuked.
}

bool PrefStore::GetFromWhitelist(const std::string& name,
                                 std::string* signature_out) {
  std::string key(kWhitelistPrefix);
  key.append(name);
  return prefs_->GetString(key, signature_out);
}

void PrefStore::Set(const std::string& name,
                    const std::string& value,
                    const std::string& signature) {
  std::string value_key = name + kValueField;
  std::string signature_key = name + kSignatureField;

  prefs_->SetString(value_key, value);
  prefs_->SetString(signature_key, signature);
}

bool PrefStore::Get(const std::string& name,
                    std::string* value_out,
                    std::string* signature_out) {
  CHECK(value_out);
  CHECK(signature_out);
  std::string value_key = name + kValueField;
  std::string signature_key = name + kSignatureField;
  bool rv = prefs_->GetString(value_key, value_out);
  return rv && prefs_->GetString(signature_key, signature_out);
}

bool PrefStore::Remove(const std::string& name,
                        std::string* value_out,
                        std::string* signature_out) {
  CHECK(value_out);
  CHECK(signature_out);
  std::string value_key = name + kValueField;
  std::string signature_key = name + kSignatureField;
  bool rv = RemoveOneString(value_key, value_out);
  return rv && RemoveOneString(signature_key, signature_out);
}

void PrefStore::Delete(const std::string& name) {
  std::string value_key = name + kValueField;
  std::string signature_key = name + kSignatureField;
  prefs_->Remove(value_key, NULL);
  prefs_->Remove(signature_key, NULL);
}

bool PrefStore::RemoveOneString(const std::string& key, std::string* out) {
  Value* tmp;
  if (!prefs_->Remove(key, &tmp))
    return false;
  // now, I own the removed object.

  bool rv = tmp->GetAsString(out);
  delete tmp;

  return rv;
}

}  // namespace login_manager
