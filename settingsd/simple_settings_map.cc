// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/simple_settings_map.h"

#include <base/logging.h>

#include "settingsd/identifier_utils.h"
#include "settingsd/settings_document.h"

namespace settingsd {

SimpleSettingsMap::SimpleSettingsMap() {
}

SimpleSettingsMap::~SimpleSettingsMap() {
}

const base::Value* SimpleSettingsMap::GetValue(const std::string& key) const {
  if (!utils::IsKey(key))
    return nullptr;

  auto it = prefix_document_map_.find(key);
  if (it == prefix_document_map_.end())
    return nullptr;

  return it->second->GetValue(key);
}

std::set<std::string> SimpleSettingsMap::GetActiveChildKeys(
    const std::string& prefix) const {
  std::set<std::string> keys;
  for (const auto& entry : prefix_document_map_) {
    if (utils::IsKey(entry.first))
      keys.insert(entry.first);
  }
  return keys;
}

void SimpleSettingsMap::InsertDocumentIntoSortedList(
    std::unique_ptr<const SettingsDocument> document) {
  auto pos = std::find_if(
      documents_.begin(), documents_.end(),
      [&document](const std::unique_ptr<const SettingsDocument>& pos) {
        return pos->GetVersionStamp().IsAfter(document->GetVersionStamp());
      });
  documents_.insert(pos, std::move(document));
}

void SimpleSettingsMap::InsertDocument(
    std::unique_ptr<const SettingsDocument> document) {
  // NOTE: The vector |documents_| actually has ownership of the
  // SettingsDocuments. Here we use the shared_ptr merely as a notification
  // system to inform SimpleSettingsMap of SettingsDocuments whose reference
  // count has dropped to zero. To accomplish this, we pass
  // OnDocumentUnreferenced as a deleter for std::shared_ptr<const
  // SettingsDocument>.
  std::shared_ptr<const SettingsDocument> document_ptr(
      document.get(), std::bind(&SimpleSettingsMap::OnDocumentUnreferenced,
                                this, std::placeholders::_1));

  // Convenience shortcuts.
  const VersionStamp& version_stamp = document_ptr->GetVersionStamp();

  std::set<std::string> prefixes =
      document_ptr->GetActiveChildPrefixes(kRootPrefix);
  for (const std::string& prefix : prefixes) {
    DCHECK(document_ptr->GetValue(prefix)) << "prefix " << prefix
                                           << " has no assigned value.";
    DCHECK(utils::IsKey(prefix) ||
           document_ptr->GetValue(prefix)->IsType(base::Value::TYPE_NULL))
        << prefix << " has a non-NULL value assigned to it.";

    // Check if there is already a value assigned for that prefix. If so, we
    // don't have to walk up the parent prefix space, because:
    // a) If the document to insert is later than that entry's document, no
    // delete operation has happened in the parent prefix space since the time
    // that other value was installed, since that would have wiped out that key.
    // b) If we are earlier, the present key is going to overwrite us anyways.
    auto current_entry = prefix_document_map_.find(prefix);
    if (current_entry != prefix_document_map_.end()) {
      if (current_entry->second->GetVersionStamp().IsBefore(version_stamp)) {
        // Install it.
        current_entry->second = document_ptr;

        // If it is a delete, delete all earlier child prefixes.
        if (!utils::IsKey(prefix))
          DeleteChildPrefixSpace(prefix, version_stamp);
      }
      continue;
    }

    // Ok so currently no entry exists under that prefix. The question is why.
    // Is it since there is a delete operation in effect higher up in the parent
    // prefix space? If that's the case and this delete operation is later than
    // the current document, we should not install the key. Otherwise we do.
    bool deleted = false;
    std::string current_prefix = prefix;
    do {
      current_prefix = utils::GetParentPrefix(current_prefix);
      auto delete_entry = prefix_document_map_.find(current_prefix);
      if (delete_entry != prefix_document_map_.end() &&
          delete_entry->second->GetVersionStamp().IsAfter(version_stamp)) {
        deleted = true;
      }
    } while (current_prefix != kRootPrefix);

    // If no later delete operation was found, install the prefix.
    if (!deleted) {
      prefix_document_map_[prefix] = document_ptr;

      // If it is a delete, delete all earlier child prefixes.
      if (!utils::IsKey(prefix))
        DeleteChildPrefixSpace(prefix, version_stamp);
    }
  }

  // Insert the document into the sorted vector of active documents only if it
  // is currently providing a setting.
  if (!document_ptr.unique())
    InsertDocumentIntoSortedList(std::move(document));
}

void SimpleSettingsMap::OnDocumentUnreferenced(
    const SettingsDocument* document) {
  std::remove_if(
      documents_.begin(), documents_.end(),
      [&document](const std::unique_ptr<const SettingsDocument>& pos) {
        return document == pos.get();
      });
}

void SimpleSettingsMap::Clear() {
  prefix_document_map_.clear();
}

std::set<std::string> SimpleSettingsMap::GetActiveChildPrefixes(
    const std::string& prefix) const {
  std::set<std::string> result;
  for (const auto& el : utils::GetChildPrefixes(prefix, prefix_document_map_))
    result.insert(el.first);
  return result;
}

void SimpleSettingsMap::DeleteChildPrefixSpace(
    const std::string& prefix,
    const VersionStamp& upper_limit) {
  auto range = utils::GetChildPrefixes(prefix, prefix_document_map_);
  for (const auto& entry : range) {
    if (entry.second->GetVersionStamp().IsBefore(upper_limit))
      prefix_document_map_.erase(entry.first);
  }
}

}  // namespace settingsd
