// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_IMPORTER_H
#define CHAPS_OBJECT_IMPORTER_H

#include <base/file_path.h>

namespace chaps {

class ObjectPool;

// An ObjectImporter instance imports legacy or external objects into an object
// pool.
class ObjectImporter {
 public:
  virtual ~ObjectImporter() {}
  // Imports objects into the given object pool.
  //  path - The path of the current token.
  //  pool - The object pool into which object should be inserted. This pointer
  //         must not be retained or freed by the ObjectImporter instance.
  virtual bool ImportObjects(const FilePath& path, ObjectPool* pool) = 0;
};

}  // namespace

#endif  // CHAPS_OBJECT_IMPORTER_H
