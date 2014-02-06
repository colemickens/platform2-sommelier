// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/nss.h"

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "shill/logging.h"
#include "shill/minijail.h"

using base::FilePath;
using base::HexEncode;
using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

namespace {

base::LazyInstance<NSS> g_nss = LAZY_INSTANCE_INITIALIZER;
const char kCertfileBasename[] = "/tmp/nss-cert.";
const char kNSSGetCert[] = SHIMDIR "/nss-get-cert";
const char kNSSGetCertUser[] = "chronos";

}  // namespace

NSS::NSS()
    : minijail_(Minijail::GetInstance()) {
  SLOG(Crypto, 2) << __func__;
}

NSS::~NSS() {
  SLOG(Crypto, 2) << __func__;
}

// static
NSS *NSS::GetInstance() {
  return g_nss.Pointer();
}

FilePath NSS::GetPEMCertfile(const string &nickname, const vector<char> &id) {
  return GetCertfile(nickname, id, "pem");
}

FilePath NSS::GetDERCertfile(const string &nickname, const vector<char> &id) {
  return GetCertfile(nickname, id, "der");
}

FilePath NSS::GetCertfile(
    const string &nickname, const vector<char> &id, const string &type) {
  string filename =
      kCertfileBasename + StringToLowerASCII(HexEncode(&id[0], id.size()));
  vector<char *> args;
  args.push_back(const_cast<char *>(kNSSGetCert));
  args.push_back(const_cast<char *>(nickname.c_str()));
  args.push_back(const_cast<char *>(type.c_str()));
  args.push_back(const_cast<char *>(filename.c_str()));
  args.push_back(NULL);

  struct minijail *jail = minijail_->New();
  minijail_->DropRoot(jail, kNSSGetCertUser);

  int status;
  if (!minijail_->RunSyncAndDestroy(jail, args, &status)) {
    LOG(ERROR) << "Unable to spawn " << kNSSGetCert << " in a jail.";
    return FilePath();
  }

  if (!WIFEXITED(status) || WEXITSTATUS(status)) {
    LOG(ERROR) << kNSSGetCert << " failed with status " << status;
    return FilePath();
  }

  return FilePath(filename);
}

}  // namespace shill
