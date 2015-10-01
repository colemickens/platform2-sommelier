// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/simple_settings_map.h"

#include <algorithm>

#include <base/logging.h>

#include "fides/identifier_utils.h"
#include "fides/settings_document.h"

namespace fides {

SimpleSettingsMap::SimpleSettingsMap() {}

SimpleSettingsMap::~SimpleSettingsMap() {}

BlobRef SimpleSettingsMap::GetValue(const Key& key) const {
  const auto it = value_map_.find(key);
  return it == value_map_.end() ? BlobRef() : it->second->GetValue(key);
}

std::set<Key> SimpleSettingsMap::GetKeys(const Key& prefix) const {
  std::set<Key> keys;
  for (const auto& entry : utils::GetRange(prefix, value_map_))
    keys.insert(entry.first);
  return keys;
}

bool SimpleSettingsMap::HasLaterValueAssignment(
    const Key& key,
    const VersionStamp& lower_bound) {
  const auto entry = value_map_.find(key);
  return entry != value_map_.end() &&
         entry->second->GetVersionStamp().IsAfter(lower_bound);
}

bool SimpleSettingsMap::HasLaterSubtreeDeletion(
    const Key& prefix,
    const VersionStamp& lower_bound) const {
  Key current_prefix = prefix;
  do {
    const auto deletion = deletion_map_.find(current_prefix);
    if (deletion != deletion_map_.end() &&
        deletion->second->GetVersionStamp().IsAfter(lower_bound))
      return true;
    current_prefix = current_prefix.GetParent();
  } while (!current_prefix.IsRootKey());
  return false;
}

SimpleSettingsMap::DocumentWeakPtrList::iterator
SimpleSettingsMap::FindDocumentInSortedList(
    const SettingsDocument* document_ptr) {
  return std::find_if(
      documents_.begin(), documents_.end(),
      [document_ptr](const std::weak_ptr<const SettingsDocument> doc_ptr) {
        return doc_ptr.lock().get() == document_ptr;
      });
}

void SimpleSettingsMap::InsertDocumentIntoSortedList(
    std::shared_ptr<const SettingsDocument> document) {
  const auto pos = std::find_if(
      documents_.begin(), documents_.end(),
      [&document](const std::weak_ptr<const SettingsDocument>& document_ptr) {
        std::shared_ptr<const SettingsDocument> doc = document_ptr.lock();
        return doc->GetVersionStamp().IsAfter(document->GetVersionStamp());
      });
  documents_.insert(pos, document);
}

void SimpleSettingsMap::DeleteSubtree(const Key& prefix,
                                      const VersionStamp& upper_limit,
                                      std::set<Key>* modified_keys) {
  // Remove deletions.
  const auto deletion_range = utils::GetRange(prefix, deletion_map_);
  for (auto it = deletion_range.begin(); it != deletion_range.end();) {
    if (it->second->GetVersionStamp().IsBefore(upper_limit))
      deletion_map_.erase(it++);
    else
      ++it;
  }

  // Remove value assignments.
  const auto value_range = utils::GetRange(prefix, value_map_);
  for (auto it = value_range.begin(); it != value_range.end();) {
    if (it->second->GetVersionStamp().IsBefore(upper_limit)) {
      if (modified_keys)
        modified_keys->insert(it->first);
      value_map_.erase(it++);
    } else {
      ++it;
    }
  }
}

void SimpleSettingsMap::InsertDocumentSubset(
    std::shared_ptr<const SettingsDocument> document,
    const std::set<Key>& prefixes,
    std::set<Key>* modified_keys) {
  // Convenience shortcuts.
  const VersionStamp& version_stamp = document->GetVersionStamp();

  for (const auto& prefix : prefixes) {
    // Deletions are always processed first, so that value assignments in key
    // prefixes affected by a deletion in the same document are not clobbered by
    // those deletions.
    for (const auto& prefix_deletion : document->GetDeletions(prefix)) {
      // Check if there is a later deletion in place higher up in the
      // ancestorial key hierarchy which renders this deletion obsolete.
      if (!HasLaterSubtreeDeletion(prefix_deletion, version_stamp)) {
        DeleteSubtree(prefix_deletion, version_stamp, modified_keys);
        deletion_map_[prefix_deletion] = document;
      }
    }

    // After having processed all pending deletions, insert the new values into
    // |value_map_|.
    for (const auto& key : document->GetKeys(prefix)) {
      // Check if there is a later deletion in place higher up in the
      // ancestorial key hierarchy or whether there is a more recent value
      // assignment which would render this value assignment obsolete.
      if (!HasLaterSubtreeDeletion(key, version_stamp) &&
          !HasLaterValueAssignment(key, version_stamp)) {
        if (modified_keys)
          modified_keys->insert(key);
        value_map_[key] = document;
      }
    }
  }
}

bool SimpleSettingsMap::InsertDocument(
    const SettingsDocument* document,
    std::set<Key>* modified_keys,
    std::vector<const SettingsDocument*>* unreferenced_documents) {
  // |unreferenced_documents_| should be empty.
  DCHECK(unreferenced_documents_.empty());

  // Check if |document| has a prefix collision with a previously inserted,
  // concurrent document. In that case, abort.
  for (const auto& doc_ptr : documents_) {
    std::shared_ptr<const SettingsDocument> doc = doc_ptr.lock();
    if (doc->GetVersionStamp().IsConcurrent(document->GetVersionStamp()) &&
        SettingsDocument::HasOverlap(*document, *doc))
      return false;
  }

  // NOTE: The vector |documents_| actually has ownership of the
  // SettingsDocuments. Here we use the shared_ptr merely as a notification
  // system to inform SimpleSettingsMap of SettingsDocuments whose reference
  // count has dropped to zero. To accomplish this, we pass
  // OnDocumentUnreferenced as a deleter for
  // std::shared_ptr<const SettingsDocument>.
  std::shared_ptr<const SettingsDocument> document_ptr(
      document, std::bind(&SimpleSettingsMap::OnDocumentUnreferenced, this,
                          std::placeholders::_1));

  // Insert the whole document into |value_map_| and |deletion_map_|.
  std::set<Key> root_set;
  root_set.insert(Key());
  InsertDocumentSubset(document_ptr, root_set, modified_keys);

  // Insert the document into the sorted vector of active documents only if it
  // is currently providing a setting.
  if (!document_ptr.unique())
    InsertDocumentIntoSortedList(document_ptr);

  // If the document to be inserted has not become active, discarding the last
  // shared_ptr referring to it will add it to the list of unreferenced
  // documents.
  document_ptr.reset();

  // If the out parameter |unreferenced_documents| is non-null, output the list
  // of unreferenced documents.
  if (unreferenced_documents)
    std::swap(*unreferenced_documents, unreferenced_documents_);
  unreferenced_documents_.clear();

  return true;
}

void SimpleSettingsMap::RemoveDocument(
    const SettingsDocument* document_ptr,
    std::set<Key>* modified_keys,
    std::vector<const SettingsDocument*>* unreferenced_documents) {
  // |unreferenced_documents_| should be empty.
  DCHECK(unreferenced_documents_.empty());

  auto position = FindDocumentInSortedList(document_ptr);

  // If the SettingsDocument |document_ptr| is not in the list of active
  // documents, there is nothing to do.
  if (position == documents_.end())
    return;

  // Acquire a shared_ptr.
  std::shared_ptr<const SettingsDocument> document(*position);

  // Get a list of all keys from |document| currently providing values and
  // remove them from the |value_map_|.
  std::set<Key> prefixes_to_restore;
  for (auto it = value_map_.begin(); it != value_map_.end();) {
    if (it->second == document) {
      prefixes_to_restore.insert(it->first);
      if (modified_keys)
        modified_keys->insert(it->first);
      value_map_.erase(it++);
    } else {
      ++it;
    }
  }

  // Add all keys identifying prefixes which have been deleted by |document| and
  // remove them from the |deletion_map_|.
  for (auto it = deletion_map_.begin(); it != deletion_map_.end();) {
    if (it->second == document) {
      prefixes_to_restore.insert(it->first);
      deletion_map_.erase(it++);
    } else {
      ++it;
    }
  }

  // |document| should be the only remaining shared_ptr to the settings
  // document at this point.
  DCHECK_EQ(1, document.use_count());

  // Walk the sorted list of documents from the earliest document up to (but not
  // including) the current document and reinstall links in the |value_map_| and
  // |deletion_map_| which are now shining through.
  DocumentWeakPtrList::reverse_iterator current_document_ptr(position);
  for (; current_document_ptr != documents_.rend(); ++current_document_ptr) {
    std::shared_ptr<const SettingsDocument> current_document =
        current_document_ptr->lock();

    // Install the relevant prefixes into |value_map_| and |deletion_map_|.
    InsertDocumentSubset(current_document, prefixes_to_restore, modified_keys);
  }

  // Trigger clean-up of the document to be removed and addition to the list of
  // unreferenced documents by discarding the last shared_ptr referring to it.
  document.reset();

  // This should have triggered the clean-up of |document|.
  DCHECK(documents_.end() == FindDocumentInSortedList(document_ptr));

  // If the out parameter |unreferenced_documents| is non-null, output the list
  // of unreferenced documents.
  if (unreferenced_documents)
    std::swap(*unreferenced_documents, unreferenced_documents_);
  unreferenced_documents_.clear();
}

void SimpleSettingsMap::OnDocumentUnreferenced(
    const SettingsDocument* document) {
  DocumentWeakPtrList::iterator position =
      std::remove_if(documents_.begin(), documents_.end(),
                     [](const std::weak_ptr<const SettingsDocument>& doc) {
                       return doc.expired();
                     });
  documents_.erase(position, documents_.end());
  unreferenced_documents_.push_back(document);
}

void SimpleSettingsMap::Clear() {
  deletion_map_.clear();
  value_map_.clear();
}

}  // namespace fides
