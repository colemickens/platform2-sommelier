// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/settings_document_manager.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <set>
#include <string>

#include "settingsd/identifier_utils.h"
#include "settingsd/settings_document.h"
#include "settingsd/settings_keys.h"
#include "settingsd/settings_map.h"

namespace settingsd {

namespace {

// Determines which sources changed their configuration according to
// |changed_keys|. The corresponding source IDs are added to
// |sources_to_revalidate|.
void UpdateSourceValidationQueue(
    const std::set<Key>& changed_keys,
    std::priority_queue<std::string>* sources_to_revalidate) {
  const Key source_prefix(Key({keys::kSettingsdPrefix, keys::kSources}));
  std::string last_source_id;
  for (Key source_key : utils::GetRange(source_prefix, changed_keys)) {
    // TODO(mnissler): handle nested sources properly.
    Key source_suffix;
    if (!source_key.Suffix(source_prefix, &source_suffix)) {
      NOTREACHED() << "Bad source key " << source_key.ToString();
      continue;
    }
    const std::string source_id = source_suffix.Split(nullptr).ToString();
    if (source_id != last_source_id)
      sources_to_revalidate->push(source_id);
    last_source_id = source_id;
  }
}

}  // namespace

SettingsDocumentManager::SourceMapEntry::SourceMapEntry(
    const std::string& source_id)
    : source_(source_id) {}

SettingsDocumentManager::SourceMapEntry::~SourceMapEntry() {}

SettingsDocumentManager::SettingsDocumentManager(
    const SourceDelegateFactoryFunction& source_delegate_factory_function,
    std::unique_ptr<SettingsMap> settings_map,
    std::unique_ptr<SettingsDocument> trusted_document)
    : source_delegate_factory_function_(source_delegate_factory_function),
      trusted_document_(std::move(trusted_document)),
      settings_map_(std::move(settings_map)) {
  // The trusted document should have an empty version stamp.
  CHECK(!VersionStamp().IsBefore(trusted_document_->GetVersionStamp()));

  settings_map_->Clear();

  // Insert the trusted document.
  std::set<Key> changed_keys;
  CHECK(settings_map_->InsertDocument(trusted_document_.get(), &changed_keys));
  UpdateTrustConfiguration(&changed_keys);
}

SettingsDocumentManager::~SettingsDocumentManager() {}

const base::Value* SettingsDocumentManager::GetValue(const Key& key) const {
  return settings_map_->GetValue(key);
}

const std::set<Key> SettingsDocumentManager::GetKeys(const Key& prefix) const {
  return settings_map_->GetKeys(prefix);
}

void SettingsDocumentManager::AddSettingsObserver(SettingsObserver* observer) {
  observers_.AddObserver(observer);
}

void SettingsDocumentManager::RemoveSettingsObserver(
    SettingsObserver* observer) {
  observers_.RemoveObserver(observer);
}

SettingsDocumentManager::InsertionStatus
SettingsDocumentManager::InsertDocument(
    std::unique_ptr<SettingsDocument> document,
    const std::string& source_id) {
  auto source_iter = sources_.find(source_id);
  CHECK(sources_.end() != source_iter);
  SourceMapEntry& entry = source_iter->second;

  // Find the insertion point.
  auto insertion_point = std::find_if(
      entry.documents_.begin(), entry.documents_.end(),
      [&document, &source_id](const std::unique_ptr<SettingsDocument>& doc) {
        return doc->GetVersionStamp().Get(source_id) >=
               document->GetVersionStamp().Get(source_id);
      });

  // The document after the insertion point needs to have a version stamp that
  // places it after the document to be inserted according to source_id's
  // component in the version stamp. This enforces that all documents from the
  // same source are in well-defined order to each other, regardless of whether
  // they overlap or not.
  if (insertion_point != entry.documents_.end()) {
    CHECK_NE(insertion_point->get(), document.get());
    if ((*insertion_point)->GetVersionStamp().Get(source_id) ==
        document->GetVersionStamp().Get(source_id)) {
      return kInsertionStatusVersionClash;
    }
  }

  // Perform access control checks.
  if (!entry.source_.CheckAccess(document.get(), kSettingStatusActive))
    return kInsertionStatusAccessViolation;

  // Everything looks good, attempt the insertion.
  std::set<Key> changed_keys;
  if (!settings_map_->InsertDocument(document.get(), &changed_keys))
    return kInsertionStatusCollision;

  entry.documents_.insert(insertion_point, std::move(document));

  // Process any trust configuration changes.
  UpdateTrustConfiguration(&changed_keys);

  FOR_EACH_OBSERVER(SettingsObserver, observers_,
                    OnSettingsChanged(changed_keys));
  return kInsertionStatusSuccess;
}

void SettingsDocumentManager::UpdateTrustConfiguration(
    std::set<Key>* changed_keys) {
  // A priority queue of sources that have pending configuration changes and
  // need to be re-parsed and their settings documents be validated. Affected
  // sources are processed in lexicographic priority order, because source
  // configuration changes may cascade: Changing a source may invalidate
  // delegations it has made, resulting in further source configuration changes.
  // However, these can only affect sources that have a lower priority than the
  // current, so they'll show up after the current source in the validation
  // queue. Hence, the code can just make one pass through affected sources in
  // order and be sure to catch all cascading changes.
  std::priority_queue<std::string> sources_to_revalidate;
  UpdateSourceValidationQueue(*changed_keys, &sources_to_revalidate);

  while (!sources_to_revalidate.empty()) {
    // Pick the highest-priority source.
    const std::string source_id = sources_to_revalidate.top();
    do {
      sources_to_revalidate.pop();
    } while (!sources_to_revalidate.empty() &&
             source_id == sources_to_revalidate.top());

    // Get or create the source map entry.
    SourceMapEntry& entry = sources_.emplace(std::piecewise_construct,
                                             std::forward_as_tuple(source_id),
                                             std::forward_as_tuple(source_id))
                                .first->second;

    // Re-parse the source configuration. If the source is no longer explicitly
    // configured, purge it (after removing its documents below).
    bool purge_source =
        !entry.source_.Update(source_delegate_factory_function_, *this);

    // Re-validate all documents belonging to this source.
    for (auto doc = entry.documents_.begin(); doc != entry.documents_.end();) {
      if (RevalidateDocument(&entry.source_, doc->get())) {
        // |doc| is still valid.
        CHECK(!purge_source);
        ++doc;
        continue;
      }

      // |doc| is no longer valid, remove it from |documents_| and
      // |settings_map_|.
      std::set<Key> keys_changed_by_removal;
      settings_map_->RemoveDocument(doc->get(), &keys_changed_by_removal);
      doc = entry.documents_.erase(doc);
      UpdateSourceValidationQueue(keys_changed_by_removal,
                                  &sources_to_revalidate);

      // Update |changed_keys| to include the keys affected by the removal.
      changed_keys->insert(keys_changed_by_removal.begin(),
                           keys_changed_by_removal.end());
    }

    if (purge_source)
      sources_.erase(source_id);
  }
}

bool SettingsDocumentManager::RevalidateDocument(
    const Source* source,
    const SettingsDocument* doc) const {
  // TODO(mnissler): Reload the document from disk and perform signature
  // validation.

  // NB: When re-validating documents, "withdrawn" status is sufficient.
  return source->CheckAccess(doc, kSettingStatusWithdrawn);
}

}  // namespace settingsd
