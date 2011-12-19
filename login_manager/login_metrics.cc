// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file handles the details of reporting user metrics related to login.

#include "login_manager/login_metrics.h"

#include <base/file_util.h>
#include <metrics/bootstat.h>
#include <metrics/metrics_library.h>

namespace login_manager {

//static
const char LoginMetrics::kLoginUserTypeMetric[] = "Login.UserType";
//static
const int LoginMetrics::kGuestUser = 0;
//static
const int LoginMetrics::kOwner = 1;
//static
const int LoginMetrics::kOther = 2;
//static
const int LoginMetrics::kDevMode = 3;
//static
const int LoginMetrics::kMaxValue = 5;

//static
const char LoginMetrics::kLoginPolicyFilesMetric[] =
    "Login.PolicyFilesStatePerBoot";
//static
const int LoginMetrics::kMaxPolicyFilesValue = 64;

// static
const char LoginMetrics::kLoginMetricsFlagFile[] = "per_boot_flag";

// static
int LoginMetrics::PolicyFilesStatusCode(const PolicyFilesStatus& status) {
  return (status.owner_key_file_state * 16      /* 4^2 */ +
          status.policy_file_state * 4          /* 4^1 */ +
          status.defunct_prefs_file_state * 1   /* 4^0 */);
}

LoginMetrics::LoginMetrics(const FilePath& per_boot_flag_dir)
    : per_boot_flag_file_(per_boot_flag_dir.Append(kLoginMetricsFlagFile)) {
  metrics_lib_.Init();
}
LoginMetrics::~LoginMetrics() {}

void LoginMetrics::SendLoginUserType(bool dev_mode, bool incognito,
                                     bool owner) {
  int uma_code = LoginUserTypeCode(dev_mode, incognito, owner);
  metrics_lib_.SendEnumToUMA(kLoginUserTypeMetric, uma_code, kMaxValue);
}

bool LoginMetrics::SendPolicyFilesStatus(const PolicyFilesStatus& status) {
  if (!file_util::PathExists(per_boot_flag_file_)) {
    metrics_lib_.SendEnumToUMA(kLoginPolicyFilesMetric,
                               LoginMetrics::PolicyFilesStatusCode(status),
                               kMaxPolicyFilesValue);
    bool created = file_util::WriteFile(per_boot_flag_file_, "", 0) == 0;
    PLOG_IF(WARNING, !created) << "Can't touch " << per_boot_flag_file_.value();
    return true;
  }
  return false;
}

void LoginMetrics::RecordStats(const char* tag) {
  bootstat_log(tag);
}

// Code for incognito, owner and any other user are 0, 1 and 2
// respectively in normal mode. In developer mode they are 3, 4 and 5.
int LoginMetrics::LoginUserTypeCode(bool dev_mode, bool guest, bool owner) {
  int code = 0;
  if (dev_mode)
    code += kDevMode;
  if (guest)
    return code + kGuestUser;
  if (owner)
    return code + kOwner;
  return code + kOther;
}

}  // namespace login_manager
