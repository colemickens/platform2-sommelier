// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_METADATA_CACHE_H_
#define SMBPROVIDER_METADATA_CACHE_H_

#include <base/macros.h>

namespace smbprovider {

// Maintains a cache of file and directory metadata. This is the data
// that is returned by stat(); name, entry type, size, date modified.
//
// The libsmbclient API can return all metadata while enumerating a
// directory, but the Chrome FileSystemProvider API makes per entry
// requests for metadata. This cache will store the results found
// when reading a directory, then use the cache to attempt to satisfy
// requests for metadata.
//
// TODO(zentaro): Follow up CL's will implement;
//    * Data structures to hold cache.
//    * Inserting items to the cache.
//    * Searching for items in the cache.
//    * Clearing the entire cache.
//    * Invalidating requested entries based on time.
//    * Purging entries based on time.
class MetadataCache {
 public:
  MetadataCache();
  ~MetadataCache();

  DISALLOW_COPY_AND_ASSIGN(MetadataCache);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_METADATA_CACHE_H_
