// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base64.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <string>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>

#include "shill/byte_string.h"
#include "shill/shims/certificates.h"

using std::string;

namespace {

const char kPEMHeader[] = "-----BEGIN CERTIFICATE-----\n";
const char kPEMFooter[] = "\n-----END CERTIFICATE-----\n";

}  // namespace

namespace shill {

namespace shims {

// static
ByteString Certificates::ConvertDERToPEM(const ByteString &der_cert) {
  char *pem_data =
      BTOA_DataToAscii(der_cert.GetConstData(), der_cert.GetLength());
  ByteString pem_cert(kPEMHeader + string(pem_data) + kPEMFooter, false);
  free(pem_data);
  return pem_cert;
}

// static
bool Certificates::Write(const ByteString &cert, const FilePath &certfile) {
  if (file_util::WriteFile(certfile,
                           reinterpret_cast<const char *>(cert.GetConstData()),
                           cert.GetLength()) !=
      static_cast<int>(cert.GetLength())) {
    file_util::Delete(certfile, false);
    LOG(ERROR) << "Unable to save certificate to a file: " << certfile.value();
    return false;
  }
  chmod(certfile.value().c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  return true;
}

}  // namespace shims

}  // namespace shill
