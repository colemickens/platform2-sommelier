// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/activating_iccid_store.h"

#include "shill/key_file_store.h"
#include "shill/logging.h"

using base::FilePath;
using std::string;

namespace shill {

const char ActivatingIccidStore::kGroupId[] = "iccid_list";
const char ActivatingIccidStore::kStorageFileName[] =
    "activating_iccid_store.profile";

namespace {

string FormattedICCID(const string &iccid) {
  return "[ICCID=" + iccid + "]";
}

string StateToString(ActivatingIccidStore::State state) {
  switch (state) {
    case ActivatingIccidStore::kStateUnknown:
      return "Unknown";
    case ActivatingIccidStore::kStatePending:
      return "Pending";
    case ActivatingIccidStore::kStateActivated:
      return "Activated";
    case ActivatingIccidStore::kStatePendingTimeout:
      return "PendingTimeout";
    default:
      return "Invalid";
  }
}

}  // namespace

ActivatingIccidStore::ActivatingIccidStore() {}
ActivatingIccidStore::~ActivatingIccidStore() {
  if (storage_.get())
    storage_->Flush();  // Make certain that everything is persisted.
}

bool ActivatingIccidStore::InitStorage(
    GLib *glib,
    const FilePath &storage_path) {
  // Close the current file.
  if (storage_.get()) {
    storage_->Flush();
    storage_.reset();  // KeyFileStore closes the file in its destructor.
  }
  if (!glib) {
    LOG(ERROR) << "Null pointer passed for |glib|.";
    return false;
  }
  if (storage_path.empty()) {
    LOG(ERROR) << "Empty storage directory path provided.";
    return false;
  }
  FilePath path = storage_path.Append(kStorageFileName);
  scoped_ptr<KeyFileStore> storage(new KeyFileStore(glib));
  storage->set_path(path);
  bool already_exists = storage->IsNonEmpty();
  if (!storage->Open()) {
    LOG(ERROR) << "Failed to open file at '" << path.AsUTF8Unsafe()  << "'";
    if (already_exists)
      storage->MarkAsCorrupted();
    return false;
  }
  if (!already_exists)
    storage->SetHeader("ICCIDs pending cellular activation.");
  storage_.reset(storage.release());
  return true;
}

ActivatingIccidStore::State ActivatingIccidStore::
    GetActivationState(const string &iccid) const {
  string formatted_iccid = FormattedICCID(iccid);
  SLOG(Cellular, 2) << __func__ << ": " << formatted_iccid;
  if (!storage_.get()) {
    LOG(ERROR) << "Underlying storage not initialized.";
    return kStateUnknown;
  }
  int state = 0;
  if (!storage_->GetInt(kGroupId, iccid, &state)) {
    SLOG(Cellular, 2) << "No entry exists for " << formatted_iccid;
    return kStateUnknown;
  }
  if (state <= 0 || state >= kStateMax) {
    SLOG(Cellular, 2) << "State value read for " << formatted_iccid
                      << " is invalid.";
    return kStateUnknown;
  }
  return static_cast<State>(state);
}

bool ActivatingIccidStore::SetActivationState(
    const string &iccid,
    State state) {
  SLOG(Cellular, 2) << __func__ << ": State=" << StateToString(state) << ", "
                    << FormattedICCID(iccid);
  if (!storage_.get()) {
    LOG(ERROR) << "Underlying storage not initialized.";
    return false;
  }
  if (state == kStateUnknown) {
    SLOG(Cellular, 2) << "kStateUnknown cannot be used as a value.";
    return false;
  }
  if (state < 0 || state >= kStateMax) {
    SLOG(Cellular, 2) << "Cannot set state to \"" << state << "\"";
    return false;
  }
  if (!storage_->SetInt(kGroupId, iccid, static_cast<int>(state))) {
    SLOG(Cellular, 2) << "Failed to store the given ICCID and state values.";
    return false;
  }
  return storage_->Flush();
}

bool ActivatingIccidStore::RemoveEntry(const std::string &iccid) {
  SLOG(Cellular, 2) << __func__ << ": " << FormattedICCID(iccid);
  if (!storage_.get()) {
    LOG(ERROR) << "Underlying storage not initialized.";
    return false;
  }
  if (!storage_->DeleteKey(kGroupId, iccid)) {
    SLOG(Cellular, 2) << "Failed to remove the given ICCID.";
    return false;
  }
  return storage_->Flush();
}

}  // namespace shill
