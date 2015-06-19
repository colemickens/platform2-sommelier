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
  const base::Value* GetValue(const std::string& key) const override;
  std::set<std::string> GetActiveChildKeys(
      const std::string& prefix) const override;
  std::set<std::string> GetActiveChildPrefixes(
      const std::string& prefix) const override;
  void InsertDocument(
      std::unique_ptr<const SettingsDocument> document) override;

  // This method gets invoked when a SettingsDocument has lost its last
  // reference from |prefix_document_map_|, i.e. is currently not providing any
  // active settings value anymore. The argument, |document|, to this method is
  // a pointer to the now unreferenced SettingsDocument. This method then
  // deletes the respective SettingsDocument by dropping the owning pointer from
  // the |documents_| member.
  void OnDocumentUnreferenced(const SettingsDocument* document);

 protected:
  // Helper method that deletes all entries in |prefix_document_map_| whose
  // keys lie in the child prefix subspace of |prefix| and where the
  // VersionStamp of the document that is currently providing them is before
  // |upper_limit|. Note that this does not include |prefix| itself.
  void DeleteChildPrefixSpace(const std::string& prefix,
                              const VersionStamp& upper_limit);

  // Inserts the document into |documents_|, the list of documents sorted by
  // their VersionStamp. Noteworthy points:
  // (1) VersionStamps fulfil the properties of vector clocks and thus allow
  // for the partial causal ordering of SettingsDocuments.
  // (2) However their properties do not suffice to define a strict weak
  // ordering, as the transitivity of equivalence is not fulfiled.
  // (3) The insertion algorithm implemented here inserts documents at the
  // latest compatible insertion point. This guarantees that all documents with
  // an is-before relationship to a document are found at lower indices.
  void InsertDocumentIntoSortedList(
      std::unique_ptr<const SettingsDocument> document);

  // The list of all active documents.
  std::vector<std::unique_ptr<const SettingsDocument>> documents_;

  // |prefix_document_map_| maps prefixes to the respective SettingsDocument
  // which is currently providing the active value. The entries in this map
  // indirectly control the lifetime of the SettingsDocument: Once the number of
  // entries in this map referring to a particular document drops to zero, the
  // OnDocumentUnreferenced method gets invoked with a pointer to that
  // SettingsDocument. This method in turn then performs the clean-up, see
  // OnDocumentUnreferenced above.
  using PrefixDocumentMap =
      std::map<std::string, std::shared_ptr<const SettingsDocument>>;
  PrefixDocumentMap prefix_document_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleSettingsMap);
};

}  // namespace settingsd

#endif  // SETTINGSD_SIMPLE_SETTINGS_MAP_H_
