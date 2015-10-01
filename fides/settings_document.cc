// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/settings_document.h"

#include <algorithm>

#include <base/logging.h>

namespace fides {

namespace {

bool Intersects(const std::set<Key>& A, const std::set<Key>& B) {
  std::set<Key>::const_iterator i = A.begin();
  std::set<Key>::const_iterator j = B.begin();
  while (i != A.end() && j != B.end()) {
    if (*i == *j)
      return true;
    else if (*i < *j)
      ++i;
    else
      ++j;
  }
  return false;
}

}  // namespace

bool SettingsDocument::HasOverlap(const SettingsDocument& A,
                                  const SettingsDocument& B) {
  Key root_key;

  // Check for value assignment collisions.
  if (Intersects(A.GetKeys(root_key), B.GetKeys(root_key)))
    return true;

  // Check whether any deletions in B affect A.
  for (auto& deletion : A.GetDeletions(root_key)) {
    if (B.HasKeysOrDeletions(deletion))
      return true;
  }

  // Check whether any deletions in A affect B.
  for (auto& deletion : B.GetDeletions(root_key)) {
    if (A.HasKeysOrDeletions(deletion))
      return true;
  }

  return false;
}

}  // namespace fides
