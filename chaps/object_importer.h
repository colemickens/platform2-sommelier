// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_IMPORTER_H
#define CHAPS_OBJECT_IMPORTER_H

namespace chaps {

class ObjectPool;

// An ObjectImporter instance imports legacy or external objects into an object
// pool.
class ObjectImporter {
 public:
  virtual ~ObjectImporter() {}

  // Imports objects into the given object pool. This method must execute as
  // quickly as possible; TPM operations should not be performed here. If TPM
  // operations are required to finish importing objects, this work should be
  // done later in FinishImportAsync.
  //  pool - The object pool into which object should be inserted. This pointer
  //         must not be retained or freed by the ObjectImporter instance.
  virtual bool ImportObjects(ObjectPool* pool) = 0;

  // Finishes importing objects that may take a long time to import. Here it is
  // safe to perform lengthy TPM operations. This is intended to be called on a
  // background thread but must not be called until ImportObjects has returned.
  //  pool - The object pool into which object should be inserted. This pointer
  //         must not be retained or freed by the ObjectImporter instance.
  virtual bool FinishImportAsync(ObjectPool* pool) = 0;
};

}  // namespace

#endif  // CHAPS_OBJECT_IMPORTER_H
