// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/settings_document_manager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "fides/identifier_utils.h"
#include "fides/locked_settings.h"
#include "fides/settings_document.h"
#include "fides/settings_keys.h"
#include "fides/settings_map.h"

namespace fides {

namespace {

// Determines which sources changed their configuration according to
// |changed_keys|. The corresponding source IDs are added to
// |sources_to_revalidate|.
void UpdateSourceValidationQueue(
    const std::set<Key>& changed_keys,
    std::priority_queue<std::string>* sources_to_revalidate) {
  const Key source_prefix(Key({keys::kFidesPrefix, keys::kSources}));
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

// A comparator for std::vector<std::unique_ptr<SettingsDocument>> that is
// sorted by the version stamp component corresponding to a specified source.
class DocumentVersionStampComparator {
 public:
  explicit DocumentVersionStampComparator(const std::string& source_id)
      : source_id_(source_id) {}

  bool operator()(const std::unique_ptr<const SettingsDocument>& doc_a,
                  const std::unique_ptr<const SettingsDocument>& doc_b) {
    return doc_a->GetVersionStamp().Get(source_id_) <
           doc_b->GetVersionStamp().Get(source_id_);
  }

 private:
  std::string source_id_;
};

}  // namespace

SettingsDocumentManager::SourceMapEntry::SourceMapEntry(
    const std::string& source_id)
    : source_(source_id) {}

SettingsDocumentManager::SourceMapEntry::~SourceMapEntry() {}

SettingsDocumentManager::SettingsDocumentManager(
    const SettingsBlobParserFunction& settings_blob_parser_function,
    const SourceDelegateFactoryFunction& source_delegate_factory_function,
    const std::string& storage_path,
    std::unique_ptr<SettingsMap> settings_map,
    std::unique_ptr<const SettingsDocument> trusted_document)
    : settings_blob_parser_function_(settings_blob_parser_function),
      source_delegate_factory_function_(source_delegate_factory_function),
      trusted_document_(std::move(trusted_document)),
      blob_store_(storage_path),
      settings_map_(std::move(settings_map)) {
  // The trusted document should have an empty version stamp.
  CHECK(!VersionStamp().IsBefore(trusted_document_->GetVersionStamp()));
}

SettingsDocumentManager::~SettingsDocumentManager() {}

void SettingsDocumentManager::Init() {
  settings_map_->Clear();

  // Insert the trusted document.
  std::set<Key> changed_keys;
  std::vector<const SettingsDocument*> unreferenced_documents;
  CHECK(settings_map_->InsertDocument(trusted_document_.get(), &changed_keys,
                                      &unreferenced_documents));
  if (!unreferenced_documents.empty())
    LOG(ERROR) << "Initial SettingsDocument is empty.";
  UpdateTrustConfiguration(&changed_keys);

  // |trusted_document| should have installed at least one source.
  if (sources_.empty())
    LOG(WARNING) << "Initial settings document has not added any sources.";

  // Load settings blobs from disk for known sources in the order of the source
  // hierarchy.
  for (auto source_iter = sources_.begin(); source_iter != sources_.end();
       ++source_iter) {
    const std::string current_source_id = source_iter->first;

    // Find all documents for the source and InsertBlob() 'em.
    for (const auto& handle : blob_store_.List(current_source_id)) {
      const std::vector<uint8_t> blob = blob_store_.Load(handle);
      InsertionStatus status = InsertBlob(current_source_id, BlobRef(&blob));
      if (status != kInsertionStatusSuccess) {
        LOG(ERROR) << "Failed to load settings blob for source "
                   << current_source_id;
      }
    }
    // Sources cannot remove themselves.
    CHECK(sources_.end() != sources_.find(current_source_id));
  }
}

BlobRef SettingsDocumentManager::GetValue(const Key& key) const {
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

SettingsDocumentManager::InsertionStatus SettingsDocumentManager::InsertBlob(
    const std::string& source_id,
    BlobRef blob) {
  const Source* source = FindSource(source_id);
  if (!source)
    return kInsertionStatusUnknownSource;

  // Parse and validate the blob.
  std::unique_ptr<LockedSettingsContainer> container;
  SettingsDocumentManager::InsertionStatus status =
      ParseAndValidateBlob(source, blob, &container);
  if (status != kInsertionStatusSuccess)
    return status;

  // Blob validation looks good. Unwrap the SettingsDocument and insert it.
  std::unique_ptr<SettingsDocument> document(
      LockedSettingsContainer::DecodePayload(std::move(container)));
  if (!document)
    return kInsertionStatusBadPayload;
  document->source_id_ = source_id;

  // Write the blob to the BlobStore.
  BlobStore::Handle handle = blob_store_.Store(source_id, blob);
  if (!handle.IsValid())
    return kInsertionStatusStorageFailure;

  document->handle_ = handle;
  status = InsertDocument(std::move(document));

  // If the insertion failed remove the corresponding blob from the BlobStore.
  if (status != kInsertionStatusSuccess && !blob_store_.Purge(handle))
    LOG(ERROR) << "Failed to purge document.";

  return status;
}

SettingsDocumentManager::InsertionStatus
SettingsDocumentManager::InsertDocument(
    std::unique_ptr<const SettingsDocument> document) {
  DCHECK(!document->source_id_.empty());
  DCHECK(document->handle_.IsValid());
  auto source_iter = sources_.find(document->source_id_);
  CHECK(sources_.end() != source_iter);
  SourceMapEntry& entry = source_iter->second;

  // Make sure that all documents from the same source are in well-defined order
  // to each other, regardless of whether they overlap or not.
  const auto insertion_point = std::lower_bound(
      entry.documents_.begin(), entry.documents_.end(), document,
      DocumentVersionStampComparator(document->source_id_));
  if (insertion_point != entry.documents_.end() &&
      (*insertion_point)->GetVersionStamp().Get(document->source_id_) ==
          document->GetVersionStamp().Get(document->source_id_)) {
    return kInsertionStatusVersionClash;
  }

  // Perform access control checks.
  if (!entry.source_.CheckAccess(document.get(), kSettingStatusActive))
    return kInsertionStatusAccessViolation;

  // Everything looks good, attempt the insertion.
  std::set<Key> changed_keys;
  std::vector<const SettingsDocument*> unreferenced_documents;
  if (!settings_map_->InsertDocument(document.get(), &changed_keys,
                                     &unreferenced_documents)) {
    DCHECK(unreferenced_documents.empty());
    return kInsertionStatusCollision;
  }

  entry.documents_.insert(insertion_point, std::move(document));

  // Purge any unreferenced documents. This might actually include
  // |document|, if e.g. all values |document| provides are already clobbered by
  // a newer document inserted before.
  for (auto unreferenced_document : unreferenced_documents) {
    if (!PurgeBlobAndDocument(unreferenced_document))
      LOG(ERROR) << "Failed to purge unreferenced document";
  }

  // Process any trust configuration changes.
  UpdateTrustConfiguration(&changed_keys);

  FOR_EACH_OBSERVER(SettingsObserver, observers_,
                    OnSettingsChanged(changed_keys));
  return kInsertionStatusSuccess;
}

void SettingsDocumentManager::RevalidateSourceDocuments(
    const SourceMapEntry& entry,
    std::set<Key>* changed_keys,
    std::priority_queue<std::string>* sources_to_revalidate) {
  std::vector<const SettingsDocument*> obsolete_documents;
  for (auto& doc : entry.documents_) {
    if (RevalidateDocument(&entry.source_, doc.get()))
      // |doc| is still valid.
      continue;

    // |doc| is no longer valid, remove it from the |settings_map_|.
    std::set<Key> keys_changed_by_removal;
    std::vector<const SettingsDocument*> unreferenced_documents;
    settings_map_->RemoveDocument(doc.get(), &keys_changed_by_removal,
                                  &unreferenced_documents);

    // Note that the document that is being removed here, must not be removed
    // from its DocumentEntry vector, since it gets added to the list of
    // unreferenced documents anyways. Since these are deleted below this
    // would result in a double free().
    obsolete_documents.insert(obsolete_documents.end(),
                              unreferenced_documents.begin(),
                              unreferenced_documents.end());

    UpdateSourceValidationQueue(keys_changed_by_removal, sources_to_revalidate);

    // Update |changed_keys| to include the keys affected by the removal.
    changed_keys->insert(keys_changed_by_removal.begin(),
                         keys_changed_by_removal.end());
  }

  // Purge any unreferenced documents.
  for (auto obsolete_document : obsolete_documents) {
    if (!PurgeBlobAndDocument(obsolete_document))
      LOG(ERROR) << "Failed to purge unreferenced document";
  }
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
    RevalidateSourceDocuments(entry, changed_keys, &sources_to_revalidate);

    if (purge_source)
      sources_.erase(source_id);
  }
}

bool SettingsDocumentManager::PurgeBlobAndDocument(
    const SettingsDocument* document) {
  auto source_iter = sources_.find(document->source_id_);
  if (source_iter == sources_.end())
    return false;

  std::unique_ptr<const SettingsDocument> dummy_for_lower_bound_search(
      document);
  auto document_iter = std::lower_bound(
      source_iter->second.documents_.begin(),
      source_iter->second.documents_.end(), dummy_for_lower_bound_search,
      DocumentVersionStampComparator(document->source_id_));
  dummy_for_lower_bound_search.release();
  if (document_iter == source_iter->second.documents_.end())
    return false;

  BlobStore::Handle handle((*document_iter)->handle_);
  source_iter->second.documents_.erase(document_iter);
  return blob_store_.Purge(handle);
}

SettingsDocumentManager::InsertionStatus
SettingsDocumentManager::ParseAndValidateBlob(
    const Source* source,
    BlobRef blob,
    std::unique_ptr<LockedSettingsContainer>* container) const {
  // Parse the blob. Try with the formats allowed by the source. If there are no
  // formats in source config, try the default format (identified by empty
  // string).
  for (const std::string& format : source->blob_formats()) {
    *container = settings_blob_parser_function_(format, blob);
    if (*container)
      break;
  }
  if (!*container && source->blob_formats().empty())
    *container = settings_blob_parser_function_(std::string(), blob);
  if (!*container)
    return kInsertionStatusParseError;

  // Validate the blob, i.e. check its integrity and verify it's authentic
  // with respect to the corresponding source configuration.
  if (!source->delegate()->ValidateContainer(*container->get()))
    return kInsertionStatusValidationError;

  // Validate the blob's version components.
  for (auto& component : (*container)->GetVersionComponents()) {
    const Source* version_source = FindSource(component->GetSourceId());
    if (!version_source ||
        !version_source->delegate()->ValidateVersionComponent(*component)) {
      return kInsertionStatusValidationError;
    }
  }
  return kInsertionStatusSuccess;
}

bool SettingsDocumentManager::RevalidateDocument(
    const Source* source,
    const SettingsDocument* doc) const {
  // Load corresponding blob from the BlobStore.
  const std::vector<uint8_t> blob = blob_store_.Load(doc->handle_);

  // Parse and validate the document against the SourceDelegate. |container| is
  // discarded without being used in this method, as we are only interested in
  // whether the LockedSettingsContainer can be validated.
  std::unique_ptr<LockedSettingsContainer> container;
  if (ParseAndValidateBlob(source, BlobRef(&blob), &container) !=
      kInsertionStatusSuccess) {
    return false;
  }

  // NB: When re-validating documents, "withdrawn" status is sufficient.
  return source->CheckAccess(doc, kSettingStatusWithdrawn);
}

const Source* SettingsDocumentManager::FindSource(
    const std::string& source_id) const {
  auto source_iter = sources_.find(source_id);
  return source_iter == sources_.end() ? nullptr : &source_iter->second.source_;
}

}  // namespace fides
