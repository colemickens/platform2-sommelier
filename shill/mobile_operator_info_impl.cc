// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mobile_operator_info_impl.h"

#include <regex.h>

#include <algorithm>
#include <cctype>
#include <map>

#include <base/bind.h>
#include <google/protobuf/repeated_field.h>

#include "shill/logging.h"
#include "shill/protobuf_lite_streams.h"

using base::Bind;
using base::FilePath;
using google::protobuf::io::CopyingInputStreamAdaptor;
using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;
using shill::mobile_operator_db::Data;
using shill::mobile_operator_db::Filter;
using shill::mobile_operator_db::LocalizedName;
using shill::mobile_operator_db::MobileAPN;
using shill::mobile_operator_db::MobileNetworkOperator;
using shill::mobile_operator_db::MobileOperatorDB;
using shill::mobile_operator_db::MobileVirtualNetworkOperator;
using shill::mobile_operator_db::OnlinePortal;
using std::map;
using std::string;
using std::vector;

namespace shill {

// static
const char * const MobileOperatorInfoImpl::kDefaultDatabasePaths[] =
    {"/usr/share/shill/serviceproviders.pbf",
     "/usr/share/shill/additional_providers.pbf"};
const int MobileOperatorInfoImpl::kMCCMNCMinLen = 5;

namespace {

// Wrap some low level functions from the GNU regex librarly.
string GetRegError(int code, const regex_t *compiled) {
  size_t length = regerror(code, compiled, NULL, 0);
  scoped_ptr<char[]> buffer(new char[length]);
  DCHECK_EQ(length, regerror(code, compiled, buffer.get(), length));
  return buffer.get();
}

}  // namespace

MobileOperatorInfoImpl::MobileOperatorInfoImpl(EventDispatcher *dispatcher)
    : dispatcher_(dispatcher),
      observers_(
          ObserverList<MobileOperatorInfo::Observer, true>::NOTIFY_ALL),
      current_mno_(nullptr),
      current_mvno_(nullptr),
      requires_roaming_(false),
      user_olp_empty_(true),
      weak_ptr_factory_(this) {
  for (const auto &database_path : kDefaultDatabasePaths) {
    AddDatabasePath(FilePath(database_path));
  }
}

MobileOperatorInfoImpl::~MobileOperatorInfoImpl() {}

void MobileOperatorInfoImpl::ClearDatabasePaths() {
  database_paths_.clear();
}

void MobileOperatorInfoImpl::AddDatabasePath(const FilePath &absolute_path) {
  database_paths_.push_back(absolute_path);
}

bool MobileOperatorInfoImpl::Init() {
  ScopedVector<MobileOperatorDB> databases;

  // |database_| is guaranteed to be set once |Init| is called.
  database_.reset(new MobileOperatorDB());

  for (const auto &database_path : database_paths_) {
    const char *database_path_cstr = database_path.value().c_str();
    scoped_ptr<CopyingInputStreamAdaptor> database_stream;
    database_stream.reset(
        protobuf_lite_file_input_stream(database_path_cstr));
    if (!database_stream.get()) {
      LOG(ERROR) << "Failed to read mobile operator database: "
                  << database_path_cstr;
      continue;
    }

    scoped_ptr<MobileOperatorDB> database(new MobileOperatorDB());
    if (!database->ParseFromZeroCopyStream(database_stream.get())) {
      LOG(ERROR) << "Could not parse mobile operator database: "
                  << database_path_cstr;
      continue;
    }
    LOG(INFO) << "Successfully loaded database: " << database_path_cstr;
    // Hand over ownership to the vector.
    databases.push_back(database.release());
  }

  // Collate all loaded databases into one.
  if (databases.size() == 0) {
    LOG(ERROR) << "Could not read any mobile operator database. "
                << "Will not be able to determine MVNO.";
    return false;
  }

  for (const auto &database : databases) {
    // TODO(pprabhu) This merge might be very costly. Determine if we need to
    // implement move semantics / bias the merge to use the largest database
    // as the base database and merge other databases into it.
    database_->MergeFrom(*database);
  }
  PreprocessDatabase();
  return true;
}

void MobileOperatorInfoImpl::AddObserver(
    MobileOperatorInfo::Observer *observer) {
  observers_.AddObserver(observer);
}

void MobileOperatorInfoImpl::RemoveObserver(
    MobileOperatorInfo::Observer *observer) {
  observers_.RemoveObserver(observer);
}

bool MobileOperatorInfoImpl::IsMobileNetworkOperatorKnown() const {
  return (current_mno_ != nullptr);
}

bool MobileOperatorInfoImpl::IsMobileVirtualNetworkOperatorKnown() const {
  return (current_mvno_ != nullptr);
}

// ///////////////////////////////////////////////////////////////////////////
// Getters.
const string &MobileOperatorInfoImpl::uuid() const {
  return uuid_;
}

const string &MobileOperatorInfoImpl::operator_name() const {
  // TODO(pprabhu) I'm not very sure yet what is the right thing to do here.
  // It is possible that we obtain a name OTA, and then using some other
  // information (say the iccid range), determine that this is an MVNO. In
  // that case, we may want to *override* |user_operator_name_| by the name
  // obtained from the DB for the MVNO.
  return operator_name_;
}

const string &MobileOperatorInfoImpl::country() const {
  return country_;
}

const string &MobileOperatorInfoImpl::mccmnc() const {
  return mccmnc_;
}

const string &MobileOperatorInfoImpl::MobileOperatorInfoImpl::sid() const {
  return sid_;
}

const string &MobileOperatorInfoImpl::nid() const {
  return (user_nid_ == "") ? nid_ : user_nid_;
}

const vector<string> &MobileOperatorInfoImpl::mccmnc_list() const {
  return mccmnc_list_;
}

const vector<string> &MobileOperatorInfoImpl::sid_list() const {
  return sid_list_;
}

const vector<MobileOperatorInfo::LocalizedName> &
MobileOperatorInfoImpl::operator_name_list() const {
  return operator_name_list_;
}

const ScopedVector<MobileOperatorInfo::MobileAPN> &
MobileOperatorInfoImpl::apn_list() const {
  return apn_list_;
}

const vector<MobileOperatorInfo::OnlinePortal> &
MobileOperatorInfoImpl::olp_list() const {
  return olp_list_;
}

const string &MobileOperatorInfoImpl::activation_code() const {
  return activation_code_;
}

bool MobileOperatorInfoImpl::requires_roaming() const {
  return requires_roaming_;
}

// ///////////////////////////////////////////////////////////////////////////
// Functions used to notify this object of operator data changes.
void MobileOperatorInfoImpl::UpdateIMSI(const string &imsi) {
  bool operator_changed = false;
  if (user_imsi_ == imsi) {
    return;
  }

  user_imsi_ = imsi;

  if (!user_mccmnc_.empty()) {
    if (user_mccmnc_.size() > imsi.size() ||
        imsi.substr(user_mccmnc_.size()) != user_mccmnc_) {
      LOG(WARNING) << "IMSI [" << imsi << "] is not a substring of the "
                    << "MCCMNC [" << user_mccmnc_ << "].";
    }
  } else {
    // Attempt to determine the MNO from IMSI since MCCMNC is absent.
    StringToMNOListMap::const_iterator cit;
    string mccmnc;

    DCHECK(candidates_by_mccmnc_.empty());
    AppendToCandidatesByMCCMNC(imsi.substr(0, kMCCMNCMinLen));
    AppendToCandidatesByMCCMNC(imsi.substr(0, kMCCMNCMinLen + 1));

    if (!candidates_by_mccmnc_.empty()) {
      // We found some candidates using IMSI.
      operator_changed |= UpdateMNO();
    }
  }
  operator_changed |= UpdateMVNO();

  // No special notification should be sent for this property, since the object
  // does not expose |imsi| as a property at all.
  if (operator_changed) {
    PostNotifyOperatorChanged();
  }
}

void MobileOperatorInfoImpl::UpdateICCID(const string &iccid) {
  if (user_iccid_ == iccid) {
    return;
  }

  user_iccid_ = iccid;
  // |iccid| is not an exposed property, so don't raise event for just this
  // property update.
  if(UpdateMVNO()) {
    PostNotifyOperatorChanged();
  }
}

void MobileOperatorInfoImpl::UpdateMCCMNC(const string &mccmnc) {
  if (user_mccmnc_ == mccmnc) {
    return;
  }

  user_mccmnc_ = mccmnc;
  HandleMCCMNCUpdate();
  candidates_by_mccmnc_.clear();
  AppendToCandidatesByMCCMNC(mccmnc);

  // Always update M[V]NO, even if we found no candidates, since we might have
  // lost some candidates due to an incorrect MCCMNC.
  bool operator_changed = false;
  operator_changed |= UpdateMNO();
  operator_changed |= UpdateMVNO();
  if (operator_changed || ShouldNotifyPropertyUpdate()) {
    PostNotifyOperatorChanged();
  }
}

void MobileOperatorInfoImpl::UpdateSID(const string &sid) {
  if (user_sid_ == sid) {
    return;
  }

  user_sid_ = sid;
  HandleSIDUpdate();
  if (UpdateMVNO() || ShouldNotifyPropertyUpdate()) {
    PostNotifyOperatorChanged();
  }
}

void MobileOperatorInfoImpl::UpdateNID(const string &nid) {
  if (user_nid_ == nid) {
    return;
  }

  user_nid_ = nid;
  if (UpdateMVNO() || ShouldNotifyPropertyUpdate()) {
    PostNotifyOperatorChanged();
  }
}

void MobileOperatorInfoImpl::UpdateOperatorName(const string &operator_name) {
  bool operator_changed = false;
  if (user_operator_name_ == operator_name) {
    return;
  }

  user_operator_name_ = operator_name;
  HandleOperatorNameUpdate();

  // We must update the candidates by name anyway.
  StringToMNOListMap::const_iterator cit = name_to_mnos_.find(operator_name);
  candidates_by_name_.clear();
  if (cit != name_to_mnos_.end()) {
    candidates_by_name_ = cit->second;
    // We should never have inserted an empty vector into the map.
    DCHECK(!candidates_by_name_.empty());
  } else {
    LOG(INFO) << "Operator name [" << operator_name << "] does not match any "
               << "MNO.";
  }

  operator_changed |= UpdateMNO();
  operator_changed |= UpdateMVNO();
  if (operator_changed || ShouldNotifyPropertyUpdate()) {
    PostNotifyOperatorChanged();
  }
}

void MobileOperatorInfoImpl::UpdateOnlinePortal(const string &url,
                                                const string &method,
                                                const string &post_data) {
  if (!user_olp_empty_ &&
      user_olp_.url == url &&
      user_olp_.method == method &&
      user_olp_.post_data == post_data) {
    return;
  }

  user_olp_empty_ = false;
  user_olp_.url = url;
  user_olp_.method = method;
  user_olp_.post_data = post_data;
  HandleOnlinePortalUpdate();

  // OnlinePortal is never used in deciding M[V]NO.
  if (ShouldNotifyPropertyUpdate()) {
    PostNotifyOperatorChanged();
  }
}

void MobileOperatorInfoImpl::Reset() {
  bool should_notify = current_mno_ != nullptr || current_mvno_ != nullptr;

  current_mno_ = nullptr;
  current_mvno_ = nullptr;
  candidates_by_mccmnc_.clear();
  candidates_by_name_.clear();

  ClearDBInformation();

  user_imsi_.clear();
  user_iccid_.clear();
  user_mccmnc_.clear();
  user_sid_.clear();
  user_nid_.clear();
  user_operator_name_.clear();
  user_olp_empty_ = true;
  user_olp_.url.clear();
  user_olp_.method.clear();
  user_olp_.post_data.clear();

  if (should_notify) {
    PostNotifyOperatorChanged();
  }
}

void MobileOperatorInfoImpl::PreprocessDatabase() {
  SLOG(Cellular, 3) << __func__;

  mccmnc_to_mnos_.clear();
  name_to_mnos_.clear();

  const RepeatedPtrField<MobileNetworkOperator> &mnos = database_->mno();
  for (const auto &mno : mnos) {
    // MobileNetworkOperator::data is a required field.
    DCHECK(mno.has_data());
    const Data &data = mno.data();

    const RepeatedPtrField<string> &mccmncs = data.mccmnc();
    for (const auto &mccmnc : mccmncs) {
      InsertIntoStringToMNOListMap(mccmnc_to_mnos_, mccmnc, &mno);
    }

    const RepeatedPtrField<LocalizedName> &localized_names =
        data.localized_name();
    for (const auto &localized_name : localized_names) {
      // LocalizedName::name is a required field.
      DCHECK(localized_name.has_name());
      InsertIntoStringToMNOListMap(name_to_mnos_,
                                   localized_name.name(),
                                   &mno);
    }
  }

  if (database_->imvno_size() > 0) {
    // TODO(pprabhu) Support IMVNOs.
    LOG(ERROR) << "InternationalMobileVirtualNetworkOperators are not "
                << "supported yet. Ignoring all IMVNOs.";
  }
}

// This function assumes that duplicate |values| are never inserted for the
// same |key|. If you do that, the function is too dumb to deduplicate the
// |value|s, and two copies will get stored.
void MobileOperatorInfoImpl::InsertIntoStringToMNOListMap(
    StringToMNOListMap &table,
    const string &key,
    const MobileNetworkOperator *value) {

  if (table.find(key) == table.end()) {
    vector<const MobileNetworkOperator *> empty_mno_list;
    table[key] = empty_mno_list;  // |empty_mno_list| copied.
  }
  table[key].push_back(value);
}

bool MobileOperatorInfoImpl::AppendToCandidatesByMCCMNC(const string &mccmnc) {
  StringToMNOListMap::const_iterator cit = mccmnc_to_mnos_.find(mccmnc);
  if (cit == mccmnc_to_mnos_.end()) {
    LOG(WARNING) << "Unknown MCCMNC value [" << mccmnc << "].";
    return false;
  }

  // We should never have inserted an empty vector into the map.
  DCHECK(!cit->second.empty());
  for (const auto mno : cit->second) {
    candidates_by_mccmnc_.push_back(mno);
  }
  return true;
}

bool MobileOperatorInfoImpl::UpdateMNO() {
  SLOG(Cellular, 3) << __func__;
  const MobileNetworkOperator *candidate = nullptr;
  if (candidates_by_mccmnc_.size() == 1) {
    candidate = candidates_by_mccmnc_[0];
    if (candidates_by_name_.size() > 0) {
      bool found_match = false;
      for (auto candidate_by_name : candidates_by_name_) {
        if (candidate_by_name == candidate) {
          found_match = true;
          break;
        }
      }
      if (!found_match) {
        SLOG(Cellular, 1) << "MNO determined by mccmnc["
                          << user_mccmnc_
                          << "] does not match any suggested by name["
                          << user_operator_name_
                          << "]. mccmnc overrides name!";
      }
    }
  } else if (candidates_by_mccmnc_.size() > 1) {
    // Try to find an intersection of the two candidate lists. These lists
    // should be almost always of length 1. Simply iterate.
    for (auto candidate_by_mccmnc : candidates_by_mccmnc_) {
      for (auto candidate_by_name : candidates_by_name_) {
        if (candidate_by_mccmnc == candidate_by_name) {
          candidate = candidate_by_mccmnc;
          break;
        }
      }
      if (candidate != nullptr) {
        break;
      }
    }
    if (candidate == nullptr) {
      SLOG(Cellular, 1) << "MNOs suggested by mccmnc ["
                        << user_mccmnc_
                        << "] are multiple and disjoint from those suggested "
                        << "by name["
                        << user_operator_name_
                        << "]. Can't make a decision.";
    }
  } else {  // candidates_by_mccmnc_.size() == 0
    if (!user_mccmnc_.empty()) {
      // Special case: In case we had a *wrong* |user_mccmnc_| update, we want
      // to override the suggestions from |user_operator_name_|. We should not
      // determine an MNO in this case.
      SLOG(Cellular, 1) << "A non-matching MCCMNC was reported by the user."
                        << "We fail the MNO match in this case.";
    } else if (candidates_by_name_.size() == 1) {
      candidate = candidates_by_name_[0];
    } else if (candidates_by_name_.size() > 1) {
      SLOG(Cellular, 1) << "Multiple MNOs suggested by name["
                        << user_operator_name_
                        << "], and none by MCCMNC. Can't make a decision.";
    } else {  // candidates_by_name_.size() == 0
      SLOG(Cellular, 1) << "No candidates suggested.";
    }
  }

  if (candidate != current_mno_) {
    current_mno_ = candidate;
    RefreshDBInformation();
    return true;
  }
  return false;
}

bool MobileOperatorInfoImpl::UpdateMVNO() {
  SLOG(Cellular, 3) << __func__;
  if (current_mno_ == nullptr) {
    return false;
  }

  for (const auto &candidate_mvno : current_mno_->mvno()) {
    bool passed_all_filters = true;
    for (const auto &filter : candidate_mvno.mvno_filter()) {
      string to_match;
      switch (filter.type()) {
        case mobile_operator_db::Filter_Type_IMSI:
          to_match = user_imsi_;
          break;
        case mobile_operator_db::Filter_Type_ICCID:
          to_match = user_iccid_;
          break;
        case mobile_operator_db::Filter_Type_SID:
          to_match = user_sid_;
          break;
        case mobile_operator_db::Filter_Type_OPERATOR_NAME:
          to_match = user_operator_name_;
          break;
        default:
          to_match.clear();
          SLOG(Cellular, 1) << "Unknown filter type [" << filter.type()
                            << "]";
          break;
      }
      if (to_match.empty()) {
        // Not enough information to pass this filter.
        passed_all_filters = false;
        break;
      }
      DCHECK(filter.has_regex());

      // Must use GNU regex implementation, since C++11 implementation is
      // incomplete.
      regex_t filter_regex;
      string filter_regex_str = filter.regex();

      // |regexec| matches the given regular expression to a substring of the
      // given query string. Ensure that |filter_regex_str| uses anchors to
      // accept only a full match.
      if (filter_regex_str.front() != '^') {
        filter_regex_str = "^" + filter_regex_str;
      }
      if (filter_regex_str.back() != '$') {
        filter_regex_str = filter_regex_str + "$";
      }

      int regcomp_error = regcomp(&filter_regex,
                                  filter_regex_str.c_str(),
                                  REG_EXTENDED | REG_NOSUB);
      if(regcomp_error) {
        LOG(WARNING) << "Could not compile regex '" << filter.regex() << "'. "
                     << "Error returned: "
                     << GetRegError(regcomp_error, &filter_regex) << ". "
                     << "Skipping current MVNO.";
        passed_all_filters = false;
        regfree(&filter_regex);
        break;
      }

      int regexec_error = regexec(&filter_regex,
                                  to_match.c_str(),
                                  0,
                                  NULL,
                                  0);
      if (regexec_error) {
        string error_string;
        error_string = GetRegError(regcomp_error, &filter_regex);
        LOG(WARNING) << "Could not match string " << to_match << " "
                     << "against regexp " << filter.regex() << ". "
                     << "Error returned: " << error_string << ". "
                     << "Skipping current MVNO.";
        passed_all_filters = false;
        regfree(&filter_regex);
        break;
      }
      regfree(&filter_regex);
    }
    if (passed_all_filters) {
      if (current_mvno_ == &candidate_mvno) {
        return false;
      }
      current_mvno_ = &candidate_mvno;
      RefreshDBInformation();
      return true;
    }
  }

  // We did not find any valid MVNO.
  if (current_mvno_ != nullptr) {
    current_mvno_ = nullptr;
    RefreshDBInformation();
    return true;
  }
  return false;
}

void MobileOperatorInfoImpl::RefreshDBInformation() {
  ClearDBInformation();

  if (current_mno_ == nullptr) {
    return;
  }

  // |data| is a required field.
  DCHECK(current_mno_->has_data());
  SLOG(Cellular, 2) << "Reloading MNO data.";
  ReloadData(current_mno_->data());

  if (current_mvno_ != nullptr) {
    // |data| is a required field.
    DCHECK(current_mvno_->has_data());
    SLOG(Cellular, 2) << "Reloading MVNO data.";
    ReloadData(current_mvno_->data());
  }
}

void MobileOperatorInfoImpl::ClearDBInformation() {
  uuid_.clear();
  country_.clear();
  nid_.clear();
  mccmnc_list_.clear();
  HandleMCCMNCUpdate();
  sid_list_.clear();
  HandleSIDUpdate();
  operator_name_list_.clear();
  HandleOperatorNameUpdate();
  apn_list_.clear();
  olp_list_.clear();
  HandleOnlinePortalUpdate();
  activation_code_.clear();
  requires_roaming_ = false;
}

void MobileOperatorInfoImpl::ReloadData(const Data &data) {
  SLOG(Cellular, 3) << __func__;
  // |uuid_| is *always* overwritten. An MNO and MVNO should not share the
  // |uuid_|.
  uuid_ = GenerateUUID(data);

  if (data.has_country()) {
    country_ = data.country();
  }

  if (data.localized_name_size() > 0) {
    operator_name_list_.clear();
    for (const auto &localized_name : data.localized_name()) {
      operator_name_list_.push_back({localized_name.name(),
                                     localized_name.language()});
    }
    HandleOperatorNameUpdate();
  }

  if (data.has_requires_roaming()) {
    requires_roaming_ = data.requires_roaming();
  }

  if (data.olp_size() > 0) {
    olp_list_.clear();
    for (const auto &olp : data.olp()) {
      // TODO(pprabhu): Support SID filters.
      // Is this supported in the old way of doing things?
      olp_list_.push_back(MobileOperatorInfo::OnlinePortal {
          olp.url(),
          (olp.method() == olp.GET) ? "GET" : "POST",
          olp.post_data()});
    }
    HandleOnlinePortalUpdate();
  }

  if (data.mccmnc_size() > 0) {
    mccmnc_list_.clear();
    for (const auto &mccmnc : data.mccmnc()) {
      mccmnc_list_.push_back(mccmnc);
    }
    HandleMCCMNCUpdate();
  }

  if (data.mobile_apn_size() > 0) {
    apn_list_.clear();
    for (const auto &apn_data : data.mobile_apn()) {
      auto *apn = new MobileOperatorInfo::MobileAPN();
      apn->apn = apn_data.apn();
      apn->username = apn_data.username();
      apn->password = apn_data.password();
      for (const auto &localized_name : apn_data.localized_name()) {
        apn->operator_name_list.push_back({localized_name.name(),
                                           localized_name.language()});
      }

      // Takes ownership.
      apn_list_.push_back(apn);
    }
  }

  if (data.sid_size() > 0) {
    sid_list_.clear();
    for (const auto &sid : data.sid()) {
      sid_list_.push_back(sid);
    }
    HandleSIDUpdate();
  }

  if (data.has_activation_code()) {
    activation_code_ = data.activation_code();
  }
}

string MobileOperatorInfoImpl::GenerateUUID(
    const mobile_operator_db::Data &data) const {
  string uuid;
  if (data.has_uuid()) {
    uuid = data.uuid();
  } else {
    // Generate a reliably reproducible and hopefully unique uid from other
    // information.
    if (data.mccmnc_size() > 0) {
      uuid += data.mccmnc(0);
    }
    if (data.localized_name_size() > 0) {
      DCHECK(data.localized_name(0).has_name());
      uuid += data.localized_name(0).name();
    }
    if (data.mobile_apn_size() > 0) {
      DCHECK(data.mobile_apn(0).has_apn());
      uuid += data.mobile_apn(0).apn();
    }
    if (data.sid_size() > 0) {
      uuid += data.sid(0);
    }
    if (data.nid_size() > 0) {
      uuid += data.nid(0);
    }
  }
  replace_if(uuid.begin(), uuid.end(),
             &MobileOperatorInfoImpl::UuidIllegalChar, '_');
  return uuid;
}

void MobileOperatorInfoImpl::HandleMCCMNCUpdate() {
  if (!user_mccmnc_.empty()) {
    bool append_to_list = true;
    for (const auto &mccmnc : mccmnc_list_) {
      append_to_list &= (user_mccmnc_ != mccmnc);
    }
    if (append_to_list) {
      mccmnc_list_.push_back(user_mccmnc_);
    }
  }

  if (!user_mccmnc_.empty()) {
    mccmnc_ = user_mccmnc_;
  } else if (mccmnc_list_.size() > 0) {
    mccmnc_ = mccmnc_list_[0];
  } else {
    mccmnc_.clear();
  }
}

void MobileOperatorInfoImpl::HandleOperatorNameUpdate() {
  if (!user_operator_name_.empty()) {
    bool append_user_operator_name = true;
    for (const auto &localized_name : operator_name_list_) {
      append_user_operator_name &= (user_operator_name_ != localized_name.name);
    }
    if (append_user_operator_name) {
      MobileOperatorInfo::LocalizedName localized_name {
          user_operator_name_,
          ""};
      operator_name_list_.push_back(localized_name);
    }
  }

  if (!user_operator_name_.empty()) {
    operator_name_ = user_operator_name_;
  } else if (operator_name_list_.size() > 0) {
    operator_name_ = operator_name_list_[0].name;
  } else {
    operator_name_.clear();
  }
}

void MobileOperatorInfoImpl::HandleSIDUpdate() {
  if (!user_sid_.empty()) {
    bool append_user_sid = true;
    for (const auto &sid : sid_list_) {
      append_user_sid &= (user_sid_ != sid);
    }
    if (append_user_sid) {
      sid_list_.push_back(user_sid_);
    }
  }

  if (!user_sid_.empty()) {
    sid_ = user_sid_;
  } else if (sid_list_.size() > 0) {
    sid_ = sid_list_[0];
  } else {
    sid_.clear();
  }
}

void MobileOperatorInfoImpl::HandleOnlinePortalUpdate() {
  if (!user_olp_empty_) {
    bool append_user_olp = true;
    for (const auto &olp : olp_list_) {
      append_user_olp &= (olp.url != user_olp_.url ||
                          olp.method != user_olp_.method ||
                          olp.post_data != user_olp_.post_data);
    }
    if (append_user_olp) {
      olp_list_.push_back(user_olp_);
    }
  }
}

void MobileOperatorInfoImpl::PostNotifyOperatorChanged() {
  SLOG(Cellular, 3) << __func__;
  dispatcher_->PostTask(
      Bind(&MobileOperatorInfoImpl::NotifyOperatorChanged,
           weak_ptr_factory_.GetWeakPtr()));
}

void MobileOperatorInfoImpl::NotifyOperatorChanged() {
  FOR_EACH_OBSERVER(MobileOperatorInfo::Observer,
                    observers_,
                    OnOperatorChanged());
}

bool MobileOperatorInfoImpl::ShouldNotifyPropertyUpdate() const {
  return IsMobileNetworkOperatorKnown() ||
         IsMobileVirtualNetworkOperatorKnown();
}

bool MobileOperatorInfoImpl::UuidIllegalChar(char a) {
  return (!isalnum(a) && a != '_');
}

}  // namespace shill
