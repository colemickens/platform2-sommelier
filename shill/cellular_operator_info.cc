// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements shill::CellularOperatorInfo. See cellular_operator_info.h for
// details.

#include "shill/cellular_operator_info.h"

#include <base/stl_util.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>

#include "shill/logging.h"

using base::FilePath;
using std::map;
using std::string;
using std::vector;

namespace shill {

namespace {

typedef vector<const CellularOperatorInfo::CellularOperator *>
    ConstOperatorVector;

string FormattedMCCMNC(const string &mccmnc) {
  return "[MCCMNC=" + mccmnc + "]";
}

string FormattedSID(const string &sid) {
  return "[SID=" + sid + "]";
}

template <class Collection>
const typename Collection::value_type::second_type *FindOrNull(
    const Collection &collection,
    const typename Collection::value_type::first_type &key) {
  typename Collection::const_iterator it = collection.find(key);
  if (it == collection.end())
    return NULL;
  return &it->second;
}

typedef CellularOperatorInfo COI;
bool ParseNameLine(const string &value, COI::LocalizedName *name) {
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

// Advances the file reader to the next line that is neither a line comment
// nor empty and returns it in |line|. If the end of file is reached, returns
// false.
bool AdvanceToNextValidLine(FileReader &file_reader, string *line) {
  while (file_reader.ReadLine(line)) {
    TrimWhitespaceASCII(*line, TRIM_ALL, line);
    // Skip line comments.
    if ((*line)[0] == '#' || line->empty())
      continue;
    // If line consists of a single null character, skip
    if (line->length() == 1 && (*line)[0] == '\0')
      continue;
    return true;
  }
  return false;
}

// Splits |line| into two strings, separated with the |key_value_delimiter|.
// Returns |false| if line does not contain |key_value_delimiter|, otherwise
// returns the first substring in |key| and the second substring in |value|,
// and returns true.
bool ParseKeyValue(const string &line,
                   char key_value_delimiter,
                   std::string *key,
                   std::string *value) {
  const size_t length = line.length();
  for (size_t i = 0; i < length; ++i) {
    if (line[i] == key_value_delimiter) {
      key->assign(line, 0, i);
      value->assign(line, i + 1, length-i);
      return true;
    }
  }
  LOG(ERROR) << "Badly formed line: " << line;
  return false;
}

}  // namespace

class CellularOperatorInfoImpl {
 public:
  CellularOperatorInfoImpl() {
    key_to_handler_["provider"] = new ProviderHandler(this);
    key_to_handler_["mccmnc"] = new MccmncHandler(this);
    key_to_handler_["name"] = new NameHandler(this);
    key_to_handler_["apn"] = new ApnHandler(this);
    key_to_handler_["sid"] = new SidHandler(this);
    key_to_handler_["olp"] = new OlpHandler(this);
    key_to_handler_["identifier"] = new IdentifierHandler(this);
    key_to_handler_["country"] = new CountryHandler(this);
  }

  ~CellularOperatorInfoImpl() {
    STLDeleteContainerPairSecondPointers(key_to_handler_.begin(),
                                         key_to_handler_.end());
  }

 private:
  struct ParserState {
    ParserState() : provider(NULL), apn(NULL), parsing_apn(false) {}

    void Clear() {
      country.clear();
      provider = NULL;
      apn = NULL;
      parsing_apn = false;
    }

    std::string country;
    COI::CellularOperator *provider;
    COI::MobileAPN *apn;

    bool parsing_apn;
  };

  class KeyHandler {
   public:
    KeyHandler(CellularOperatorInfoImpl *info_impl) : info_impl_(info_impl) {}
    virtual ~KeyHandler() {}

    virtual bool HandleKey(const string &value) = 0;

   protected:
    CellularOperatorInfoImpl *info_impl() const { return info_impl_; }

   private:
    CellularOperatorInfoImpl *info_impl_;

    DISALLOW_COPY_AND_ASSIGN(KeyHandler);
  };

  class CountryHandler : public KeyHandler {
   public:
    CountryHandler(CellularOperatorInfoImpl *info_impl)
        : KeyHandler(info_impl) {}

    bool HandleKey(const string &value) {
      return info_impl()->HandleCountry(value);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(CountryHandler);
  };

  class NameHandler : public KeyHandler {
   public:
    NameHandler(CellularOperatorInfoImpl *info_impl)
        : KeyHandler(info_impl) {}

    bool HandleKey(const string &value) {
      return info_impl()->HandleNameKey(value);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(NameHandler);
  };

  class ProviderHandler : public KeyHandler {
   public:
    ProviderHandler(CellularOperatorInfoImpl *info_impl)
        : KeyHandler(info_impl) {}

    bool HandleKey(const string &value) {
      return info_impl()->HandleProvider(value);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(ProviderHandler);
  };

  class MccmncHandler : public KeyHandler {
   public:
    MccmncHandler(CellularOperatorInfoImpl *info_impl)
        : KeyHandler(info_impl) {}

    bool HandleKey(const string &value) {
      return info_impl()->HandleMCCMNC(value);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(MccmncHandler);
  };

  class IdentifierHandler : public KeyHandler {
   public:
    IdentifierHandler(CellularOperatorInfoImpl *info_impl)
        : KeyHandler(info_impl) {}

    bool HandleKey(const string &value) {
      return info_impl()->HandleIdentifier(value);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(IdentifierHandler);
  };

  class ApnHandler : public KeyHandler {
   public:
    ApnHandler(CellularOperatorInfoImpl *info_impl)
        : KeyHandler(info_impl) {}

    bool HandleKey(const string &value) {
      return info_impl()->HandleAPN(value);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(ApnHandler);
  };

  class SidHandler : public KeyHandler {
   public:
    SidHandler(CellularOperatorInfoImpl *info_impl)
        : KeyHandler(info_impl) {}

    bool HandleKey(const string &value) {
      return info_impl()->HandleSID(value);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(SidHandler);
  };

  class OlpHandler : public KeyHandler {
   public:
    OlpHandler(CellularOperatorInfoImpl *info_impl)
        : KeyHandler(info_impl) {}

    bool HandleKey(const string &value) {
      return info_impl()->HandleOLP(value);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(OlpHandler);
  };

  bool HandleFirstLine(FileReader &reader) {
    // Read until the first line that is not a line comment.
    string line;
    if (!AdvanceToNextValidLine(reader, &line))
      return false;

    string key, value;
    if (!ParseKeyValue(line, ':', &key, &value))
      return false;

    if (key != "serviceproviders") {
      LOG(ERROR) << "File does not begin with \"serviceproviders\" "
                 << "entry.";
      return false;
    }
    if (value != "3.0") {
      LOG(ERROR) << "Unrecognized serviceproviders format.";
      return false;
    }
    return true;
  }

  bool HandleCountry(const string &value) {
    state_.country = value;
    return true;
  }

  bool HandleNameKey(const string &value) {
    if (state_.parsing_apn)
      return HandleAPNName(value);
    return HandleName(value);
  }

  bool HandleProvider(const string &value) {
    state_.parsing_apn = false;

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
    state_.provider = new COI::CellularOperator();
    state_.provider->is_primary_ = is_primary != 0;
    state_.provider->requires_roaming_ = requires_roaming != 0;
    state_.provider->country_ = state_.country;

    operators_.push_back(state_.provider);
    return true;
  }

  bool HandleMCCMNC(const string &value) {
    if (value.empty()) {
      LOG(ERROR) << "Empty \"mccmnc\" value.";
      return false;
    }
    if (!state_.provider) {
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
        state_.provider->mccmnc_list_.push_back(mccmnc);
        mccmnc_to_operator_[mccmnc] = state_.provider;
        int index = -1;
        if (base::StringToInt(mccmnc_list[i + 1], &index))
          state_.provider->mccmnc_to_olp_idx_[mccmnc] = index;
      }
    }
    return true;
  }

  bool HandleIdentifier(const string &value) {
    if (!state_.provider) {
      LOG(ERROR) << "Found \"identifier\" entry without \"provider\".";
      return false;
    }
    state_.provider->identifier_ = value;
    return true;
  }

  bool HandleAPN(const string &value) {
    if (!state_.provider) {
      LOG(ERROR) << "Found \"apn\" entry without \"provider\".";
      return false;
    }
    vector<string> fields;
    base::SplitString(value, ',', &fields);
    if (fields.size() != 4) {
      LOG(ERROR) << "Badly formed \"apn\" entry.";
      return false;
    }
    state_.apn = new COI::MobileAPN();
    state_.apn->apn = fields[1];
    state_.apn->username = fields[2];
    state_.apn->password = fields[3];
    state_.provider->apn_list_.push_back(state_.apn);
    state_.parsing_apn = true;
    return true;
  }

  bool HandleAPNName(const string &value) {
    if (!(state_.parsing_apn && state_.apn)) {
      LOG(ERROR) << "APN not being parsed.";
      return false;
    }
    COI::LocalizedName name;
    if (!ParseNameLine(value, &name))
      return false;
    state_.apn->name_list.push_back(name);
    return true;
  }

  bool HandleName(const string &value) {
    if (!state_.provider) {
      LOG(ERROR) << "Found \"name\" entry without \"provider\".";
      return false;
    }

    COI::LocalizedName name;
    if (!ParseNameLine(value, &name))
      return false;

    if (state_.provider->name_list_.size() == 0) {
      ConstOperatorVector &matching_operators =
          name_to_operators_[name.name];
      matching_operators.push_back(state_.provider);
    }
    state_.provider->name_list_.push_back(name);
    return true;
  }

  bool HandleSID(const string &value) {
    if (value.empty()) {
      LOG(ERROR) << "Empty \"sid\" value.";
      return false;
    }
    if (!state_.provider) {
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
        state_.provider->sid_list_.push_back(sid);
        sid_to_operator_[sid] = state_.provider;
        int index = -1;
        if (base::StringToInt(sid_list[i + 1], &index))
          state_.provider->sid_to_olp_idx_[sid] = index;
      }
    }
    return true;
  }

  bool HandleOLP(const string &value) {
    if (!state_.provider) {
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

    state_.provider->olp_list_.push_back(olp);
    return true;
  }

  void ClearOperators() {
    operators_.clear();
    mccmnc_to_operator_.clear();
    sid_to_operator_.clear();
    name_to_operators_.clear();
  }

  bool HandleKeyValue(const string &key, const string &value) {
    KeyHandler * const *handler = FindOrNull(key_to_handler_, key);
    if (!handler) {
      LOG(ERROR) << "Invalid key \"" << key << "\".";
      return false;
    }
    return (*handler)->HandleKey(value);
  }

  bool Load(const FilePath &info_file_path) {
    // Clear any previus operators.
    ClearOperators();

    FileReader file_reader;
    if (!file_reader.Open(info_file_path)) {
      LOG(ERROR) << "Could not open operator info file.";
      return false;
    }

    // See data/cellular_operator_info for the format of file contents.

    state_.Clear();

    if (!HandleFirstLine(file_reader))
      return false;

    string line;
    while (true) {
      if (!AdvanceToNextValidLine(file_reader, &line))
        break;

      string key, value;
      if (!ParseKeyValue(line, ':', &key, &value)) {
        ClearOperators();
        return false;
      }

      if (!HandleKeyValue(key, value)) {
        LOG(ERROR) << "Failed to parse \"" << key << "\" entry.";
        ClearOperators();
        return false;
      }
    }
    return true;
  }

 private:
  friend class CellularOperatorInfo;

  ParserState state_;

  ScopedVector<COI::CellularOperator> operators_;
  map<string, COI::CellularOperator *> mccmnc_to_operator_;
  map<string, COI::CellularOperator *> sid_to_operator_;
  map<string, ConstOperatorVector> name_to_operators_;

  map<string, KeyHandler *> key_to_handler_;

  DISALLOW_COPY_AND_ASSIGN(CellularOperatorInfoImpl);
};

COI::LocalizedName::LocalizedName() {}
COI::LocalizedName::LocalizedName(string name,
                                  string language)
    : name(name),
      language(language) {}

CellularOperatorInfo::CellularOperator::CellularOperator()
    : is_primary_(false),
      requires_roaming_(false) {}

CellularOperatorInfo::CellularOperatorInfo()
    : impl_(new CellularOperatorInfoImpl()) {}
CellularOperatorInfo::~CellularOperatorInfo() {}

const ScopedVector<CellularOperatorInfo::CellularOperator> &
CellularOperatorInfo::operators() const {
  return impl_->operators_;
}

const CellularOperatorInfo::CellularOperator *
CellularOperatorInfo::GetCellularOperatorByMCCMNC(const string &mccmnc) const {
  SLOG(Cellular, 2) << __func__ << "(" << FormattedMCCMNC(mccmnc) << ")";

  CellularOperator * const *provider =
      FindOrNull(impl_->mccmnc_to_operator_, mccmnc);
  if (!provider) {
    LOG(ERROR) << "Operator with " << FormattedMCCMNC(mccmnc)
               << " not found.";
    return NULL;
  }
  return *provider;
}

const CellularOperatorInfo::CellularOperator *
CellularOperatorInfo::GetCellularOperatorBySID(const string &sid) const {
  SLOG(Cellular, 2) << __func__ << "(" << FormattedSID(sid) << ")";

  CellularOperator * const *provider = FindOrNull(impl_->sid_to_operator_, sid);
  if (!provider) {
    LOG(ERROR) << "Operator with " << FormattedSID(sid) << " not found.";
    return NULL;
  }
  return *provider;
}

const ConstOperatorVector *CellularOperatorInfo::GetCellularOperators(
    const string &name) const {
  SLOG(Cellular, 2) << __func__ << "(" << name << ")";

  const ConstOperatorVector *providers =
      FindOrNull(impl_->name_to_operators_, name);
  if (!providers) {
    LOG(ERROR) << "Given name \"" << name << "\" did not match any operators.";
    return NULL;
  }
  return providers;
}

const CellularService::OLP *
CellularOperatorInfo::GetOLPByMCCMNC(const string &mccmnc) const {
  SLOG(Cellular, 2) << __func__ << "(" << FormattedMCCMNC(mccmnc) << ")";

  const CellularOperator *provider = GetCellularOperatorByMCCMNC(mccmnc);
  if (!provider)
    return NULL;

  const uint32 *indexptr = FindOrNull(provider->mccmnc_to_olp_idx_, mccmnc);
  if (!indexptr)
    return NULL;

  uint32 index = *indexptr;
  if (index >= provider->olp_list_.size()) {
    LOG(ERROR) << "Invalid OLP index found for "
               << FormattedMCCMNC(mccmnc) << ".";
    return NULL;
  }

  return provider->olp_list_[index];
}

const CellularService::OLP *
CellularOperatorInfo::GetOLPBySID(const string &sid) const {
  SLOG(Cellular, 2) << __func__ << "(" << FormattedSID(sid) << ")";

  const CellularOperator *provider = GetCellularOperatorBySID(sid);
  if (!provider)
    return NULL;

  const uint32 *indexptr = FindOrNull(provider->sid_to_olp_idx_, sid);
  if (!indexptr)
    return NULL;

  uint32 index = *indexptr;
  if (index >= provider->olp_list_.size()) {
    LOG(ERROR) << "Invalid OLP index found for " << FormattedSID(sid) << ".";
    return NULL;
  }

  return provider->olp_list_[index];
}

bool CellularOperatorInfo::Load(const FilePath &info_file_path) {
  SLOG(Cellular, 2) << __func__ << "(" << info_file_path.value() << ")";
  return impl_->Load(info_file_path);
}

}  // namespace shill
