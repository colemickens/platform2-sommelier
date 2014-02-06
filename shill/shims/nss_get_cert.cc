// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cert.h>
#include <nspr.h>
#include <nss.h>

#include <string>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>

#include "shill/byte_string.h"
#include "shill/shims/certificates.h"

using shill::ByteString;
using shill::shims::Certificates;
using std::string;

namespace {

const char kCertDBDir[] = "sql:/home/chronos/user/.pki/nssdb";

class ScopedNSS {
 public:
  ScopedNSS() : initialized_(false) {}
  ~ScopedNSS();

  bool Init(const string &config_dir);

 private:
  bool initialized_;
};

ScopedNSS::~ScopedNSS() {
  if (initialized_) {
    NSS_Shutdown();
    initialized_ = false;
  }
}

bool ScopedNSS::Init(const string &config_dir) {
  if (!initialized_ && (NSS_Init(config_dir.c_str()) != SECSuccess)) {
    LOG(ERROR) << "Unable to initialize NSS in " << config_dir
               << ". Error code: " << PR_GetError();
    return false;
  }
  initialized_ = true;
  return true;
}

bool GetDERCertificate(const string &nickname, ByteString *der_cert) {
  CERTCertDBHandle *handle = CERT_GetDefaultCertDB();
  if (!handle) {
    LOG(ERROR) << "Null certificate database handle.";
    return false;
  }
  CERTCertificate *cert = CERT_FindCertByNickname(handle, nickname.c_str());
  if (!cert) {
    LOG(ERROR) << "Couldn't find certificate: " << nickname;
    return false;
  }
  *der_cert = ByteString(cert->derCert.data, cert->derCert.len);
  CERT_DestroyCertificate(cert);
  return true;
}

}  // namespace

int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);
  if (argc != 4) {
    LOG(ERROR) << "Usage: nss-get-cert <cert-nickname> <der|pem> <outfile>";
    return EXIT_FAILURE;
  }

  const string nickname = argv[1];
  const string format_str = argv[2];
  const base::FilePath outfile(argv[3]);

  ScopedNSS nss;
  ByteString cert;
  if (!nss.Init(kCertDBDir) || !GetDERCertificate(nickname, &cert)) {
    return EXIT_FAILURE;
  }
  if (format_str == "pem") {
    cert = Certificates::ConvertDERToPEM(cert);
  } else if (format_str != "der") {
    LOG(ERROR) << "Invalid format parameter: " << format_str;
    return EXIT_FAILURE;
  }
  return Certificates::Write(cert, outfile) ? EXIT_SUCCESS : EXIT_FAILURE;
}
