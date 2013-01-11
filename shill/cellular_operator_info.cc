// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_operator_info.h"

#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>

#include "shill/file_reader.h"
#include "shill/logging.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

namespace {

const char kKeyOLPURL[] = "OLP.URL";
const char kKeyOLPMethod[] = "OLP.Method";
const char kKeyOLPPostData[] = "OLP.PostData";

}  // namespace

CellularOperatorInfo::CellularOperator::CellularOperator()
    : is_primary_(false),
      requires_roaming_(false) {}
CellularOperatorInfo::CellularOperator::~CellularOperator() {}

CellularOperatorInfo::MobileAPN::MobileAPN() {}
CellularOperatorInfo::MobileAPN::~MobileAPN() {}

CellularOperatorInfo::CellularOperatorInfo() {}
CellularOperatorInfo::~CellularOperatorInfo() {}

const ScopedVector<CellularOperatorInfo::CellularOperator> &
CellularOperatorInfo::operators() const {
  return operators_;
}

// static
string CellularOperatorInfo::FormattedMCCMNC(const string &mccmnc) {
  return "[MCCMNC=" + mccmnc + "]";
}

// static
string CellularOperatorInfo::FormattedSID(const string &sid) {
  return "[SID=" + sid + "]";
}

const CellularOperatorInfo::CellularOperator *
CellularOperatorInfo::GetCellularOperatorByMCCMNC(const string &mccmnc) {
  LOG(INFO) << __func__ << "(" << FormattedMCCMNC(mccmnc) << ")";

  map<string, CellularOperator *>::const_iterator it =
      mccmnc_to_operator_.find(mccmnc);
  if (it == mccmnc_to_operator_.end()) {
    LOG(ERROR) << "Operator with " << FormattedMCCMNC(mccmnc)
               << " not found.";
    return NULL;
  }

  return it->second;
}

const CellularOperatorInfo::CellularOperator *
CellularOperatorInfo::GetCellularOperatorBySID(const string &sid) {
  LOG(INFO) << __func__ << "(" << FormattedSID(sid) << ")";

  map<string, CellularOperator *>::const_iterator it =
      sid_to_operator_.find(sid);
  if (it == sid_to_operator_.end()) {
    LOG(ERROR) << "Operator with " << FormattedSID(sid) << " not found.";
    return NULL;
  }

  return it->second;
}

const vector<const CellularOperatorInfo::CellularOperator *> *
CellularOperatorInfo::GetCellularOperatorsByName(const string &name) {
  LOG(INFO) << __func__ << "(" << name << ")";

  map<string, vector<const CellularOperator *> >::const_iterator it =
      name_to_operators_.find(name);
  if (it == name_to_operators_.end()) {
    LOG(ERROR) << "Given name \"" << name << "\" did not match any operators.";
    return NULL;
  }

  return &it->second;
}

const CellularService::OLP *
CellularOperatorInfo::GetOLPByMCCMNC(const string &mccmnc) {
  LOG(INFO) << __func__ << "(" << FormattedMCCMNC(mccmnc) << ")";

  const CellularOperator *provider = GetCellularOperatorByMCCMNC(mccmnc);
  if (!provider)
    return NULL;

  map<string, uint32>::const_iterator it =
    provider->mccmnc_to_olp_idx_.find(mccmnc);

  if (it == provider->mccmnc_to_olp_idx_.end())
    return NULL;

  uint32 index = it->second;
  if (index >= provider->olp_list_.size()) {
    LOG(ERROR) << "Invalid OLP index found for "
               << FormattedMCCMNC(mccmnc) << ".";
    return NULL;
  }

  return provider->olp_list_[index];
}

const CellularService::OLP *
CellularOperatorInfo::GetOLPBySID(const string &sid) {
  LOG(INFO) << __func__ << "(" << FormattedSID(sid) << ")";

  const CellularOperator *provider = GetCellularOperatorBySID(sid);
  if (!provider)
    return NULL;

  map<string, uint32>::const_iterator it =
    provider->sid_to_olp_idx_.find(sid);

  if (it == provider->sid_to_olp_idx_.end())
    return NULL;

  uint32 index = it->second;
  if (index >= provider->olp_list_.size()) {
    LOG(ERROR) << "Invalid OLP index found for " << FormattedSID(sid) << ".";
    return NULL;
  }

  return provider->olp_list_[index];
}

void CellularOperatorInfo::ClearOperators() {
  operators_.reset();
  mccmnc_to_operator_.clear();
  sid_to_operator_.clear();
  name_to_operators_.clear();
}

// static
bool CellularOperatorInfo::ParseKeyValue(const string &line,
                                         char key_value_delimiter,
                                         std::string *key,
                                         std::string *value) {
  size_t length = line.length();
  for (size_t i = 0; i < length; ++i) {
    if (line[i] == key_value_delimiter) {
      key->assign(line, 0, i);
      value->assign(line, i + 1, length-i);
      return true;
    }
  }
  return false;
}

bool CellularOperatorInfo::Load(const FilePath &info_file_path) {
  LOG(INFO) << __func__ << "(" << info_file_path.value() << ")";

  // Clear any previus operators.
  ClearOperators();

  FileReader file_reader;
  if (!file_reader.Open(info_file_path)) {
    LOG(ERROR) << "Could not open operator info file.";
    return false;
  }

  // See data/cellular_operator_info for the format of file contents.

  ParserState state;

  typedef bool (CellularOperatorInfo::*KeyHandlerFunc)
      (ParserState *, const string &);
  map<string, KeyHandlerFunc> key_handler_map;
  key_handler_map["provider"] = &CellularOperatorInfo::HandleProvider;
  key_handler_map["mccmnc"] = &CellularOperatorInfo::HandleMCCMNC;
  key_handler_map["name"] = &CellularOperatorInfo::HandleNameKey;
  key_handler_map["apn"] = &CellularOperatorInfo::HandleAPN;
  key_handler_map["sid"] = &CellularOperatorInfo::HandleSID;
  key_handler_map["olp"] = &CellularOperatorInfo::HandleOLP;
  key_handler_map["country"] = &CellularOperatorInfo::HandleCountry;

  string line;
  bool firstline = true;
  while (file_reader.ReadLine(&line)) {
    // Skip line comments.
    TrimWhitespaceASCII(line, TRIM_ALL, &line);
    if (line[0] == '#' || line.empty())
      continue;

    // If line consists of a single null character, skip
    if (line.length() == 1 && line[0] == '\0')
      continue;

    string key, value;
    if (!ParseKeyValue(line, ':', &key, &value)) {
      LOG(ERROR) << "Badly formed line: " << line;
      return false;
    }

    if (firstline) {
      if (key != "serviceproviders") {
        LOG(ERROR) << "File does not begin with \"serviceproviders\" "
                   << "entry.";
        return false;
      }
      if (value != "3.0") {
        LOG(ERROR) << "Unrecognized serviceproviders format.";
        return false;
      }
      firstline = false;
    } else {
      map<string, KeyHandlerFunc>::iterator it = key_handler_map.find(key);
      if (it == key_handler_map.end()) {
        LOG(ERROR) << "Invalid key \"" << key << "\".";
        return false;
      }
      KeyHandlerFunc func = it->second;
      if (!(this->*func)(&state, value)) {
        LOG(ERROR) << "Failed to parse \"" << key << "\" entry.";
        return false;
      }
    }
  }
  return true;
}

bool CellularOperatorInfo::HandleCountry(ParserState *state,
                                         const string &value) {
  state->country = value;
  return true;
}

bool CellularOperatorInfo::HandleNameKey(ParserState *state,
                                         const string &value) {
  if (state->parsing_apn)
    return HandleAPNName(state, value);
  return HandleName(state, value);
}

bool CellularOperatorInfo::HandleProvider(ParserState *state,
                                          const string &value) {
  state->parsing_apn = false;

  vector<string> fields;
  base::SplitString(value, ',', &fields);
  if (fields.size() != 4) {
    LOG(ERROR) << "Badly formed \"provider\" entry.";
    return false;
  }

  int is_primary = 0;
  if (!base::StringToInt(fields[2], &is_primary)) {
    LOG(ERROR) << "Badly formed value for \"is_primary\": " << fields[2];
    return false;
  }
  int requires_roaming = 0;
  if (!base::StringToInt(fields[3], &requires_roaming)) {
    LOG(ERROR) << "Badly formed value for \"requires_roaming\": "
               << fields[3];
    return false;
  }
  state->provider = new CellularOperator();
  state->provider->is_primary_ = is_primary != 0;
  state->provider->requires_roaming_ = requires_roaming != 0;
  state->provider->country_ = state->country;

  operators_.push_back(state->provider);
  return true;
}

bool CellularOperatorInfo::HandleMCCMNC(ParserState *state,
                                        const string &value) {
  if (value.empty()) {
    LOG(ERROR) << "Empty \"mccmnc\" value.";
    return false;
  }
  if (!state->provider) {
    LOG(ERROR) << "Found \"mccmnc\" entry without \"provider\".";
    return false;
  }

  vector<string> mccmnc_list;
  base::SplitString(value, ',', &mccmnc_list);
  if ((mccmnc_list.size() % 2) != 0) {
    LOG(ERROR) << "Badly formatted \"mccmnc\" entry. "
               << "Expected even number of elements.";
    return false;
  }

  for (size_t i = 0; i < mccmnc_list.size(); i += 2) {
    const string &mccmnc = mccmnc_list[i];
    if (!mccmnc.empty()) {
      state->provider->mccmnc_list_.push_back(mccmnc);
      mccmnc_to_operator_[mccmnc] = state->provider;
      int index = -1;
      if (base::StringToInt(mccmnc_list[i + 1], &index))
        state->provider->mccmnc_to_olp_idx_[mccmnc] = index;
    }
  }
  return true;
}

bool CellularOperatorInfo::HandleAPN(ParserState *state,
                                     const string &value) {
  if (!state->provider) {
    LOG(ERROR) << "Found \"apn\" entry without \"provider\".";
    return false;
  }
  vector<string> fields;
  base::SplitString(value, ',', &fields);
  if (fields.size() != 4) {
    LOG(ERROR) << "Badly formed \"apn\" entry.";
    return false;
  }
  state->apn = new MobileAPN();
  state->apn->apn_ = fields[1];
  state->apn->username_ = fields[2];
  state->apn->password_ = fields[3];
  state->provider->apn_list_.push_back(state->apn);
  state->parsing_apn = true;
  return true;
}

bool CellularOperatorInfo::HandleAPNName(ParserState *state,
                                         const string &value) {
  if (!(state->parsing_apn && state->apn)) {
    LOG(ERROR) << "APN not being parsed.";
    return false;
  }
  LocalizedName name;
  if (!ParseNameLine(value, &name))
    return false;
  state->apn->name_list_.push_back(name);
  return true;
}

bool CellularOperatorInfo::HandleName(ParserState *state,
                                      const string &value) {
  if (!state->provider) {
    LOG(ERROR) << "Found \"name\" entry without \"provider\".";
    return false;
  }
  LocalizedName name;
  if (!ParseNameLine(value, &name))
    return false;
  if (state->provider->name_list_.size() == 0) {
    vector<const CellularOperator *> &matching_operators =
        name_to_operators_[name.name];
    matching_operators.push_back(state->provider);
  }
  state->provider->name_list_.push_back(name);
  return true;
}

bool CellularOperatorInfo::ParseNameLine(const string &value,
                                         LocalizedName *name) {
  vector<string> fields;
  base::SplitString(value, ',', &fields);
  if (fields.size() != 2) {
    LOG(ERROR) << "Badly formed \"name\" entry.";
    return false;
  }
  name->language = fields[0];
  name->name = fields[1];
  return true;
}

bool CellularOperatorInfo::HandleSID(ParserState *state,
                                     const string &value) {
  if (value.empty()) {
    LOG(ERROR) << "Empty \"sid\" value.";
    return false;
  }
  if (!state->provider) {
    LOG(ERROR) << "Found \"sid\" entry without \"provider\".";
    return false;
  }
  vector<string> sid_list;
  base::SplitString(value, ',', &sid_list);
  if ((sid_list.size() % 2) != 0) {
    LOG(ERROR) << "Badly formatted \"sid\" entry. "
               << "Expected even number of elements. ";
    return false;
  }

  for (size_t i = 0; i < sid_list.size(); i += 2) {
    const string &sid = sid_list[i];
    if (!sid.empty()) {
      state->provider->sid_list_.push_back(sid);
      sid_to_operator_[sid] = state->provider;
      int index = -1;
      if (base::StringToInt(sid_list[i + 1], &index))
        state->provider->sid_to_olp_idx_[sid] = index;
    }
  }
  return true;
}

bool CellularOperatorInfo::HandleOLP(ParserState *state,
                                     const string &value) {
  if (!state->provider) {
    LOG(ERROR) << "Found \"olp\" entry without \"provider\"";
    return false;
  }
  vector<string> fields;
  base::SplitString(value, ',', &fields);
  if (fields.size() != 3) {
    LOG(ERROR) << "Badly formed \"apn\" entry.";
    return false;
  }
  CellularService::OLP *olp = new CellularService::OLP();
  olp->SetMethod(fields[0]);
  olp->SetURL(fields[1]);
  olp->SetPostData(fields[2]);

  state->provider->olp_list_.push_back(olp);
  return true;
}

}  // namespace shill
