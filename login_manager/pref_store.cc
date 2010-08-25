// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/pref_store.h"

#include <base/json/json_writer.h>
#include <base/file_util.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/values.h>

namespace login_manager {

// static
const char PrefStore::kWhitelistPrefix[] = "whitelist";
// static
const char PrefStore::kPropertiesPrefix[] = "properties";
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
  // These will all get blown away if we get a valid dictionary.
  prefs_.reset(new DictionaryValue);
  whitelist_ = new DictionaryValue;
  properties_ = new DictionaryValue;
  // prefs_ takes ownership of whitelist_ and properties_ here.
  prefs_->Set(kWhitelistPrefix, whitelist_);
  prefs_->Set(kPropertiesPrefix, properties_);

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
  scoped_ptr<Value> new_val(base::JSONReader::ReadAndReturnError(json,
                                                                 false,
                                                                 &error_code,
                                                                 &error));
  if (!new_val.get()) {
    LOG(ERROR) << "Could not parse prefs off disk: " << error;
    return false;
  }
  if (!new_val->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Prefs file wasn't a dictionary!";
    return false;
  }
  DictionaryValue* new_dict = static_cast<DictionaryValue*>(new_val.get());
  if (!new_dict->GetDictionary(kWhitelistPrefix, &whitelist_)) {
    LOG(ERROR) << "Prefs file had no whitelist sub-dictionary.";
    return false;
  }
  if (!new_dict->GetDictionary(kPropertiesPrefix, &properties_)) {
    LOG(ERROR) << "Prefs file had no properties sub-dictionary.";
    return false;
  }

  prefs_.reset(static_cast<DictionaryValue*>(new_val.release()));
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
  StringValue* signature_value = new StringValue(signature);
  whitelist_->SetWithoutPathExpansion(name, signature_value);
}

void PrefStore::Unwhitelist(const std::string& name) {
  // Passing NULL causes the value to get nuked.
  whitelist_->RemoveWithoutPathExpansion(name, NULL);
}

bool PrefStore::GetFromWhitelist(const std::string& name,
                                 std::string* signature_out) {
  return whitelist_->GetStringWithoutPathExpansion(name, signature_out);
}

void PrefStore::Set(const std::string& name,
                    const std::string& value,
                    const std::string& signature) {
  std::string value_key = name + kValueField;
  std::string signature_key = name + kSignatureField;

  properties_->SetString(value_key, value);
  properties_->SetString(signature_key, signature);
}

bool PrefStore::Get(const std::string& name,
                    std::string* value_out,
                    std::string* signature_out) {
  CHECK(value_out);
  CHECK(signature_out);
  std::string value_key = name + kValueField;
  std::string signature_key = name + kSignatureField;
  bool rv = properties_->GetString(value_key, value_out);
  return rv && properties_->GetString(signature_key, signature_out);
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
  properties_->Remove(value_key, NULL);
  properties_->Remove(signature_key, NULL);
}

bool PrefStore::RemoveOneString(const std::string& key, std::string* out) {
  Value* tmp;
  if (!properties_->Remove(key, &tmp))
    return false;
  // now, I own the removed object.

  bool rv = tmp->GetAsString(out);
  delete tmp;

  return rv;
}

}  // namespace login_manager
