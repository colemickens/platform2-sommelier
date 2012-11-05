// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_OPERATOR_INFO_H_
#define SHILL_CELLULAR_OPERATOR_INFO_H_

#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>

#include "shill/cellular_service.h"
#include "shill/key_file_store.h"

namespace shill {

class GLib;

class CellularOperatorInfo {
 public:
  explicit CellularOperatorInfo(GLib *glib);
  virtual ~CellularOperatorInfo();

  // Loads the operator info from |info_file_path|. Returns true on success.
  virtual bool Load(const FilePath &info_file_path);

  // Gets the online payment portal info of the operator with ID |operator_id|.
  // Returns true if the info is found.
  virtual bool GetOLP(const std::string &operator_id,
                      CellularService::OLP *olp);

 private:
  KeyFileStore info_file_;

  DISALLOW_COPY_AND_ASSIGN(CellularOperatorInfo);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_OPERATOR_INFO_H_
