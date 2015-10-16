// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/dbus_settings_service_impl.h"

#include <base/logging.h>

#include "fides/settings_document_manager.h"

namespace fides {

namespace {

// The error domain reported for dbus errors.
const char kErrorDomain[] = "fides";

// Error code for failed document insertions.
const char kErrorInsertionFailed[] = "blob_insertion_failed";

// Error messages for failed document insertions.
const char kErrorMsgInsertionVersionClash[] = "Source version already used.";
const char kErrorMsgInsertionCollision[] = "Collision with other document.";
const char kErrorMsgInsertionParseError[] = "Failed to parse the blob.";
const char kErrorMsgInsertionValidationError[] = "Blob failed validation.";
const char kErrorMsgInsertionBadPayload[] = "Failed to decode blob payload.";
const char kErrorMsgInsertionUnknownSource[] = "Blob origin unknown.";
const char kErrorMsgInsertionStorageFailure[] =
    "Failed to write the blob to BlobStore.";
const char kErrorMsgInsertionAccessViolation[] =
    "Document touches off-bounds keys.";

// Error code when asking for a key that has no value assigned.
const char kErrorNoValue[] = "get_no_value";

// Error code when asking for a key that has no value assigned.
const char kErrorMsgNoValue[] = "%s has no assigned value.";

// Error code for invalid keys.
const char kErrorInvalidKey[] = "get_invalid_key";

// Error message for invalid keys.
const char kErrorMsgInvalidKey[] =
    "%s is not a valid string representation of a key.";

const char* InsertionStatusToErrorMsg(
    SettingsDocumentManager::InsertionStatus status) {
  switch (status) {
    case SettingsDocumentManager::kInsertionStatusSuccess:
      LOG(FATAL) << "InsertionStatusToErrorMsg() called on success.";
      return nullptr;
    case SettingsDocumentManager::kInsertionStatusVersionClash:
      return kErrorMsgInsertionVersionClash;
    case SettingsDocumentManager::kInsertionStatusCollision:
      return kErrorMsgInsertionCollision;
    case SettingsDocumentManager::kInsertionStatusAccessViolation:
      return  kErrorMsgInsertionAccessViolation;
    case SettingsDocumentManager::kInsertionStatusParseError:
      return kErrorMsgInsertionParseError;
    case SettingsDocumentManager::kInsertionStatusValidationError:
      return kErrorMsgInsertionValidationError;
    case SettingsDocumentManager::kInsertionStatusBadPayload:
      return kErrorMsgInsertionBadPayload;
    case SettingsDocumentManager::kInsertionStatusStorageFailure:
      return kErrorMsgInsertionStorageFailure;
    case SettingsDocumentManager::kInsertionStatusUnknownSource:
      return kErrorMsgInsertionUnknownSource;
  }
  NOTREACHED() << "Update: Unhandled error mode.";
  return nullptr;
}

}  // namespace

DBusSettingsServiceImpl::DBusSettingsServiceImpl(
    SettingsDocumentManager* settings_document_manager,
    const base::WeakPtr<brillo::dbus_utils::ExportedObjectManager>&
        object_manager,
    const dbus::ObjectPath& object_path)
    : settings_document_manager_(settings_document_manager),
      dbus_object_(object_manager.get(),
                   object_manager->GetBus(),
                   object_path) {
  settings_document_manager_->AddSettingsObserver(this);
}

DBusSettingsServiceImpl::~DBusSettingsServiceImpl() {
  settings_document_manager_->RemoveSettingsObserver(this);
}

void DBusSettingsServiceImpl::OnSettingsChanged(const std::set<Key>& keys) {
  std::vector<std::string> changed_keys;
  changed_keys.reserve(keys.size());
  for (const auto& key : keys)
    changed_keys.push_back(key.ToString());
  dbus_adaptor_.SendOnSettingsChangedSignal(changed_keys);
}

void DBusSettingsServiceImpl::Start(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(sequencer->GetHandler(
      "DBusSettingsServiceImpl.RegisterAsync() failed.", true));
}

bool DBusSettingsServiceImpl::Get(brillo::ErrorPtr* error,
                                  const std::string& in_key,
                                  std::vector<uint8_t>* out_value) {
  if (!Key::IsValidKey(in_key)) {
    brillo::Error::AddToPrintf(error, FROM_HERE, kErrorDomain,
                                 kErrorInvalidKey, kErrorMsgInvalidKey,
                                 in_key.c_str());
    return false;
  }
  BlobRef value = settings_document_manager_->GetValue(Key(in_key));
  if (!value.valid()) {
    brillo::Error::AddToPrintf(error, FROM_HERE, kErrorDomain, kErrorNoValue,
                                 kErrorMsgNoValue, in_key.c_str());
    return false;
  }
  *out_value = value.ToVector();
  return true;
}

bool DBusSettingsServiceImpl::Enumerate(brillo::ErrorPtr* error,
                                        const std::string& in_prefix,
                                        std::vector<std::string>* out_values) {
  if (!Key::IsValidKey(in_prefix)) {
    brillo::Error::AddToPrintf(error, FROM_HERE, kErrorDomain,
                                 kErrorInvalidKey, kErrorMsgInvalidKey,
                                 in_prefix.c_str());
    return false;
  }
  const std::set<Key> keys =
      settings_document_manager_->GetKeys(Key(in_prefix));
  for (const auto& key : keys)
    out_values->push_back(key.ToString());
  return true;
}

bool DBusSettingsServiceImpl::Update(
      brillo::ErrorPtr* error,
      const std::vector<uint8_t>& in_blob,
      const std::string& in_source_id) {
  SettingsDocumentManager::InsertionStatus insertion_status =
      settings_document_manager_->InsertBlob(
          in_source_id, BlobRef(&in_blob));
  if (insertion_status != SettingsDocumentManager::kInsertionStatusSuccess) {
    brillo::Error::AddTo(error, FROM_HERE, kErrorDomain,
                           kErrorInsertionFailed,
                           InsertionStatusToErrorMsg(insertion_status));
    return false;
  }
  return true;
}

}  // namespace fides
