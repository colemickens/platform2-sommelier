// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_H
#define CHAPS_OBJECT_H

#include "chaps/attributes.h"

namespace chaps {

// Object is the interface for a PKCS #11 object.  This component manages all
// object attributes and provides query and modify access to attributes
// according to the current object policy.
class Object {
 public:
  virtual int GetSize() const = 0;
  virtual CK_RV GetAttributes(CK_ATTRIBUTE_PTR attributes,
                              int num_attributes) = 0;
  virtual CK_RV SetAttributes(const CK_ATTRIBUTE_PTR attributes,
                              int num_attributes) = 0;
  virtual bool GetAttributeBool(CK_ATTRIBUTE_TYPE type, bool default_value) = 0;
};

}  // namespace

#endif  // CHAPS_OBJECT_H
