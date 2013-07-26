// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/provider.h"

#include "shill/error.h"
#include "shill/service.h"

namespace shill {

Provider::Provider() {}

void Provider::CreateServicesFromProfile(const ProfileRefPtr &/*profile*/) {}

ServiceRefPtr Provider::FindSimilarService(
      const KeyValueStore &/*args*/, Error *error) const {
  error->Populate(Error::kInternalError,
                  "FindSimilarService is not implemented");
  return NULL;
}

ServiceRefPtr Provider::GetService(const KeyValueStore &/*args*/,
                                   Error *error) {
  error->Populate(Error::kInternalError, "GetService is not implemented");
  return NULL;
}

ServiceRefPtr Provider::CreateTemporaryService(
      const KeyValueStore &/*args*/, Error *error) {
  error->Populate(Error::kInternalError,
                  "CreateTemporaryService is not implemented");
  return NULL;
}

void Provider::Start() {}

void Provider::Stop() {}

}  // namespace shill
