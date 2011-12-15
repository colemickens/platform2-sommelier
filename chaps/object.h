// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
  virtual ~Object() {}
  // Returns a general indicator of the object's size. This size will be at
  // least as large as the combined size of the object's attribute values.
  virtual int GetSize() const = 0;
  // Provides PKCS #11 attribute values according to the semantics described in
  // PKCS #11 v2.20: 11.7 - C_GetAttributeValue (p. 133).
  virtual CK_RV GetAttributes(CK_ATTRIBUTE_PTR attributes,
                              int num_attributes) const = 0;
  // Sets object attributes from a list of PKCS #11 attribute values according
  // to the semantics described in PKCS #11 v2.20: 11.7 - C_SetAttributeValue
  // (p. 135).
  virtual CK_RV SetAttributes(const CK_ATTRIBUTE_PTR attributes,
                              int num_attributes) = 0;
  // Provides a convenient way to query a boolean attribute. If the attribute
  // does not exist or is not valid, 'default_value' is returned.
  virtual bool GetAttributeBool(CK_ATTRIBUTE_TYPE type,
                                bool default_value) const = 0;
};

}  // namespace

#endif  // CHAPS_OBJECT_H
