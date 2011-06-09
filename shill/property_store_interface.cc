// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/property_store_interface.h"

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>

#include "shill/error.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

PropertyStoreInterface::PropertyStoreInterface() {}

PropertyStoreInterface::~PropertyStoreInterface() {}

bool PropertyStoreInterface::SetBoolProperty(const std::string& name,
                                             bool value,
                                             Error *error) {
  return ReturnError(name, error);
}

bool PropertyStoreInterface::SetInt16Property(const std::string& name,
                                              int16 value,
                                              Error *error) {
  return ReturnError(name, error);
}

bool PropertyStoreInterface::SetInt32Property(const std::string& name,
                                              int32 value,
                                              Error *error) {
  return ReturnError(name, error);
}

bool PropertyStoreInterface::SetStringProperty(const std::string& name,
                                               const std::string& value,
                                               Error *error) {
  return ReturnError(name, error);
}

bool PropertyStoreInterface::SetStringmapProperty(
    const std::string& name,
    const std::map<std::string, std::string>& values,
    Error *error) {
  return ReturnError(name, error);
}

bool PropertyStoreInterface::SetStringsProperty(
    const std::string& name,
    const std::vector<std::string>& values,
    Error *error) {
  return ReturnError(name, error);
}

bool PropertyStoreInterface::SetUint8Property(const std::string& name,
                                              uint8 value,
                                              Error *error) {
  return ReturnError(name, error);
}

bool PropertyStoreInterface::SetUint16Property(const std::string& name,
                                               uint16 value,
                                               Error *error) {
  return ReturnError(name, error);
}

bool PropertyStoreInterface::SetUint32Property(const std::string& name,
                                               uint32 value,
                                               Error *error) {
  return ReturnError(name, error);
}

bool PropertyStoreInterface::ReturnError(const std::string& name,
                                         Error *error) {
  NOTIMPLEMENTED() << name << " is not writable.";
  if (error)
    error->Populate(Error::kInvalidArguments, name + " is not writable.");
  return false;
}

}  // namespace shill
