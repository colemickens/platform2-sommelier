// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_SETTINGS_MAP_H_
#define FIDES_SETTINGS_MAP_H_

#include <base/macros.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "fides/blob_ref.h"
#include "fides/settings_document.h"

namespace fides {

// Interface for accessing configuration values.
//
// SettingsDocuments contain settings as key-value pairs and allow querying
// values and enumerating settings keys. This header file defines an interface
// which offers a similar interface but provides a merged view on settings
// provided by a set of SettingsDocuments. SettingsDocuments can be partially
// ordered by the values of their respective vector clocks (see
// https://en.wikipedia.org/wiki/Vector_clock). The view offered by this
// interface reflects the "latest" state of a setting given all inserted
// SettingsDocuments. When SettingsDocuments are inserted or removed the view is
// updated accordingly. SettingsDocuments with overlapping sets of settings keys
// which are concurrent (neither before nor later according to their vector
// clocks), are said to be "colliding". Attempting to insert a SettingsDocument
// which collides with an already inserted document is considered an error,
// insertion is aborted and the SettingsMap remains unmodified.
// SimpleSettingsMap is a map-based implementation of SettingsMap, which stores
// for each key a reference to the SettingsDocument providing the latest value.
// TODO(cschuet): Write a tree-based implementation of this interface that
//                offers better complexity for document removal.
class SettingsMap {
 public:
  virtual ~SettingsMap() {}

  // Clears the settings map.
  virtual void Clear() = 0;

  // Retrieves the currently active value for the setting identified by |key|.
  // If no such setting is currently set, this method returns a nullptr. Note
  // that InsertDocument(), below, ensures that no SettingsDocument gets
  // inserted into SettingsMap that collides with an already inserted document.
  // As such, there is at most one SettingsDocument providing the latest value
  // for a |key|.
  virtual BlobRef GetValue(const Key& key) const = 0;

  // Returns the list of currently active settings whose keys have |key| as an
  // ancestor.
  virtual std::set<Key> GetKeys(const Key& prefix) const = 0;

  // Inserts a settings document into the settings map. Returns true if the
  // insertion is successful. If the document insertion fails due to a collision
  // with a previously inserted document, returns false. In this case
  // SettingsMap is unchanged. If the out parameter |modified_keys| is not the
  // |nullptr|, keys that have been added or deleted by the insertion are
  // inserted into the set. Note that this only includes all keys for which the
  // return value of GetValue() has changed. If the out parameter
  // |unreferenced_documents| is not the |nullptr|, |unreferenced_documents|
  // will be replaced with a list of documents that are now unreferenced due
  // to the insertion.
  virtual bool InsertDocument(
      const SettingsDocument* document,
      std::set<Key>* modified_keys,
      std::vector<const SettingsDocument*>* unreferenced_documents) = 0;

  // Removes a settings document from the settings map and deletes it. The
  // attempt to remove a document that not currently not contained in
  // SettingsMap is a noop. If the out parameter |modified_keys| is not the
  // |nullptr|, keys that have been added or deleted by the deletion are
  // inserted into the set. Note that this only includes all keys for which the
  // return value of GetValue() has changed. If the out parameter
  // |unreferenced_documents| is not the |nullptr|, |unreferenced_documents|
  // will be replaced with a list of documents that are now unreferenced due
  // to the document removal. Note that this will include the |document| itself.
  virtual void RemoveDocument(
      const SettingsDocument* document,
      std::set<Key>* modified_keys,
      std::vector<const SettingsDocument*>* unreferenced_documents) = 0;

 private:
  DISALLOW_ASSIGN(SettingsMap);
};

}  // namespace fides

#endif  // FIDES_SETTINGS_MAP_H_
