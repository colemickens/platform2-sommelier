// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROVIDER_INTERFACE_H_
#define SHILL_PROVIDER_INTERFACE_H_

#include "shill/refptr_types.h"

namespace shill {

class Error;
class KeyValueStore;

// This is an object that creates and manages service objects.  It provides
// default implementations for each provider method so sublcasses do not
// need to implement boilerplate unimplemented methods.
class Provider {
 public:
  virtual ~Provider() {}

  // Creates services from the entries within |profile|.
  virtual void CreateServicesFromProfile(const ProfileRefPtr &profile);

  // Find a Service with similar properties to |args|.  The criteria
  // used are specific to the provider subclass.  Returns a reference
  // to a matching service if one exists.  Otherwise it returns a NULL
  // reference and populates |error|.
  virtual ServiceRefPtr FindSimilarService(
      const KeyValueStore &args, Error *error) const;

  // Retrieves (see FindSimilarService) or creates a service with the
  // unique attributes in |args|.  The remaining attributes will be
  // populated (by Manager) via a later call to Service::Configure().
  // Returns a NULL reference and populates |error| on failure.
  virtual ServiceRefPtr GetService(const KeyValueStore &args, Error *error);

  // Create a temporary service with the identifying properties populated
  // from |args|.  Callers outside of the Provider must never register
  // this service with the Manager or connect it since it was never added
  // to the provider's service list.
  virtual ServiceRefPtr CreateTemporaryService(
      const KeyValueStore &args, Error *error);

  // Start the provider.
  virtual void Start();

  // Stop the provider (will de-register all services).
  virtual void Stop();

 protected:
  Provider();

 private:
  friend class ProviderTest;

  DISALLOW_COPY_AND_ASSIGN(Provider);
};

}  // namespace shill

#endif  // SHILL_PROVIDER_INTERFACE_H_
