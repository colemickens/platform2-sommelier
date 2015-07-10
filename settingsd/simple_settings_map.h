// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_SIMPLE_SETTINGS_MAP_H_
#define SETTINGSD_SIMPLE_SETTINGS_MAP_H_

#include <base/macros.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "settingsd/settings_map.h"
#include "settingsd/version_stamp.h"

namespace base {
class Value;
}

namespace settingsd {

class SettingsDocument;

// Simple map-based implemenation of the SettingsMap.
class SimpleSettingsMap : public SettingsMap {
 public:
  SimpleSettingsMap();
  ~SimpleSettingsMap() override;

  // SettingsMap:
  void Clear() override;
  const base::Value* GetValue(const Key& key) const override;
  std::set<Key> GetKeys(const Key& key) const override;
  bool InsertDocument(const SettingsDocument* document,
                      std::set<Key>* modified_keys) override;
  void RemoveDocument(const SettingsDocument* document,
                      std::set<Key>* modified_keys) override;

  // This method gets invoked when a SettingsDocument has lost its last
  // reference from |value_map_| and |deletion_map_|, i.e. is currently
  // providing neither any active settings value nor deletions. The argument,
  // |document|, to this method is a pointer to the now unreferenced
  // SettingsDocument.
  void OnDocumentUnreferenced(const SettingsDocument* document);

 private:
  using WeakPtrDocumentList =
      std::vector<std::weak_ptr<const SettingsDocument>>;
  using KeyDocumentMap = std::map<Key, std::shared_ptr<const SettingsDocument>>;
  friend class SimpleSettingsMapTest;

  // Helper method that deletes all entries in |value_map_| and |deletion_map_|
  // whose keys lie in the subtree rooted at |prefix| and where the VersionStamp
  // of the document that is currently providing them is before |upper_limit|.
  void DeleteSubtree(const Key& prefix,
                     const VersionStamp& upper_limit,
                     std::set<Key>* modified_keys);

  // Returns true if |key| has a value assignment later than |lower_bound|.
  // Otherwise, returns false.
  bool HasLaterValueAssignment(const Key& key, const VersionStamp& lower_bound);

  // Returns true if |prefix| has been removed by a deletion later than
  // |lower_bound|. Otherwise, return false.
  bool HasLaterSubtreeDeletion(const Key& prefix,
                               const VersionStamp& lower_bound) const;

  // Inserts the document into |documents_|, i.e. the list of documents sorted
  // by their VersionStamp. Noteworthy points:
  // (1) VersionStamps fulfil the properties of vector clocks and thus allow
  //     for the partial causal ordering of SettingsDocuments.
  // (2) However their properties do not suffice to define a strict weak
  //     ordering, as the transitivity of equivalence is not fulfiled.
  // (3) The insertion algorithm implemented here inserts documents at the
  //     latest compatible insertion point. This guarantees that all documents
  //     with an is-before relationship to a document are found at lower
  //     indices.
  void InsertDocumentIntoSortedList(
      std::shared_ptr<const SettingsDocument> document);

  // Returns an iterator to the entry in |documents_| which points the same
  // SettingsDocument as |document_ptr| does.
  WeakPtrDocumentList::iterator FindDocumentInSortedList(
      const SettingsDocument* document_ptr);

  // Installs the subset of keys and subtree deletions provided by |document|
  // for which at least one ancestor key is a member of |prefixes| into the
  // |value_map_| or |deletion_map_|. If the out parameter |modified_keys| is
  // not the |nullptr|, keys that have been added or deleted by the insertion
  // are inserted into the set. Note that this only includes currently visible
  // modifications and not those that have been clobbered by a later document
  // already present in the SettingsMap.
  void InsertDocumentSubset(std::shared_ptr<const SettingsDocument> document,
                            const std::set<Key>& prefixes,
                            std::set<Key>* modified_keys);

  // The list of all active documents.
  WeakPtrDocumentList documents_;

  // |value_map_| maps keys to the respective SettingsDocument which is
  // currently providing the active value. The entries in this map indirectly
  // control the lifetime of the SettingsDocument: Once the number of entries in
  // this map and |deletion_map_| referring to a particular document drops to
  // zero, the OnDocumentUnreferenced method gets invoked with a pointer to that
  // SettingsDocument. This method in turn then performs the clean-up, see
  // OnDocumentUnreferenced above.
  KeyDocumentMap value_map_;

  // |deletion_map_| maps keys to the respective SettingsDocument which is
  // currently providing the delete operation for that subtree. See |value_map_|
  // for comments regarding the lifetime of SettingsDocuments.
  KeyDocumentMap deletion_map_;

  DISALLOW_COPY_AND_ASSIGN(SimpleSettingsMap);
};

}  // namespace settingsd

#endif  // SETTINGSD_SIMPLE_SETTINGS_MAP_H_
