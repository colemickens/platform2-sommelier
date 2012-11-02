// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_operator_info.h"

using std::string;

namespace shill {

namespace {

const char kKeyOLPURL[] = "OLP.URL";
const char kKeyOLPMethod[] = "OLP.Method";
const char kKeyOLPPostData[] = "OLP.PostData";

}  // namespace

CellularOperatorInfo::CellularOperatorInfo(GLib *glib) : info_file_(glib) {}

CellularOperatorInfo::~CellularOperatorInfo() {}

bool CellularOperatorInfo::Load(const FilePath &info_file_path) {
  info_file_.set_path(info_file_path);
  if (!info_file_.Open()) {
    LOG(ERROR) << "Could not open cellular operator info file '"
               << info_file_path.value() << "'.";
    return false;
  }
  return true;
}

bool CellularOperatorInfo::GetOLP(const string &operator_id,
                                  CellularService::OLP *olp) {
  if (!info_file_.ContainsGroup(operator_id)) {
    LOG(INFO) << "Could not find cellular operator '" << operator_id << "'.";
    return false;
  }

  string value;
  if (!info_file_.GetString(operator_id, kKeyOLPURL, &value)) {
    LOG(INFO) << "Could not find online payment portal info for cellular "
              << "operator '" << operator_id << "'.";
    return false;
  }

  olp->SetURL(value);

  if (info_file_.GetString(operator_id, kKeyOLPMethod, &value)) {
    olp->SetMethod(value);
  } else {
    olp->SetMethod("");
  }

  if (info_file_.GetString(operator_id, kKeyOLPPostData, &value)) {
    olp->SetPostData(value);
  } else {
    olp->SetPostData("");
  }

  return true;
}

}  // namespace shill
