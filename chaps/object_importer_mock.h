// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_IMPORTER_MOCK_H
#define CHAPS_OBJECT_IMPORTER_MOCK_H

#include "chaps/object_importer.h"

#include <base/basictypes.h>
#include <gmock/gmock.h>

namespace chaps {

class ObjectImporterMock : public ObjectImporter {
 public:
  ObjectImporterMock();
  virtual ~ObjectImporterMock();

  MOCK_METHOD2(ImportObjects, bool(const FilePath&, ObjectPool*));

 private:
  DISALLOW_COPY_AND_ASSIGN(ObjectImporterMock);
};

}  // namespace chaps

#endif  // CHAPS_OBJECT_IMPORTER_MOCK_H
