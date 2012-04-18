// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/nss.h"

#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/stringprintf.h>

#include "shill/glib.h"
#include "shill/scope_logger.h"

using base::HexEncode;
using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

namespace {
const char kCertfileBasename[] = "/tmp/nss-cert.";
const char kNSSGetCertScript[] = SCRIPTDIR "/nss-get-cert";
}  // namespace

// TODO(ers): not using LAZY_INSTANCE_INITIALIZER
// because of http://crbug.com/114828
static base::LazyInstance<NSS> g_nss = { 0, {{0}} };

NSS::NSS()
    : glib_(NULL) {
  SLOG(Crypto, 2) << __func__;
}

NSS::~NSS() {
  SLOG(Crypto, 2) << __func__;
}

// static
NSS *NSS::GetInstance() {
  return g_nss.Pointer();
}

void NSS::Init(GLib *glib) {
  glib_ = glib;
}

FilePath NSS::GetPEMCertfile(const string &nickname, const vector<char> &id) {
  return GetCertfile(nickname, id, "pem");
}

FilePath NSS::GetDERCertfile(const string &nickname, const vector<char> &id) {
  return GetCertfile(nickname, id, "der");
}

FilePath NSS::GetCertfile(
    const string &nickname, const vector<char> &id, const string &type) {
  CHECK(glib_);
  string filename =
      kCertfileBasename + StringToLowerASCII(HexEncode(&id[0], id.size()));
  char *argv[] = {
    const_cast<char *>(kNSSGetCertScript),
    const_cast<char *>(nickname.c_str()),
    const_cast<char *>(type.c_str()),
    const_cast<char *>(filename.c_str()),
    NULL
  };
  char *envp[1] = { NULL };
  int status = 0;
  GError *error = NULL;
  if (!glib_->SpawnSync(NULL,
                        argv,
                        envp,
                        static_cast<GSpawnFlags>(0),
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        &status,
                        &error)) {
    LOG(ERROR) << "nss-get-cert failed: "
               << glib_->ConvertErrorToMessage(error);
    return FilePath();
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status)) {
    LOG(ERROR) << "nss-get-cert failed, status=" << status;
    return FilePath();
  }
  return FilePath(filename);
}

}  // namespace shill
