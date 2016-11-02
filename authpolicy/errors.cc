// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/errors.h"

namespace errors {

const char kDomain[] = "authpolicyd";

const char kCreateDirFailed[] = "create_dir_failed";
const char kDownloadGpoFailed[] = "download_gpo_failed";
const char kKInitFailed[] = "kinit_failed";
const char kNetAdsGpoListFailed[] = "net_ads_gpo_list_failed";
const char kNetAdsInfoFailed[] = "net_ads_info_failed";
const char kNetAdsJoinFailed[] = "net_ads_join_failed";
const char kNetAdsSearchFailed[] = "net_ads_search_failed";
const char kNotLoggedIn[] = "not_logged_in";
const char kParseGpoFailed[] = "parse_gpo_failed";
const char kParseGpoPathFailed[] = "parse_gpo_path_failed";
const char kParseNetAdsInfoFailed[] = "parse_net_ads_info_failed";
const char kParseNetAdsSearchFailed[] = "parse_net_ads_search_failed";
const char kParseSmbConfFailed[] = "parse_smb_conf_failed";
const char kParseUPNFailed[] = "parse_upn_failed";
const char kPregFileNotFound[] = "preg_file_not_found";
const char kPregParseError[] = "preg_parse_error";
const char kPregReadError[] = "preg_error";
const char kPregTooBig[] = "preg_too_big";
const char kReadSmbConfFailed[] = "read_smb_conf_failed";
const char kSmbClientFailed[] = "smd_client_failed";
const char kStorePolicyFailed[] = "store_policy_failed";
const char kWriteSmbConfFailed[] = "write_smb_conf_failed";

}  // namespace errors

