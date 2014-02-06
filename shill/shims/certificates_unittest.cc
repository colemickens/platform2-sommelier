// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shims/certificates.h"

#include <sys/stat.h>

#include <base/files/file_path.h>
#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

using std::string;
using testing::Test;

namespace shill {

namespace shims {

class CertificatesTest : public Test {
};

TEST_F(CertificatesTest, ConvertDERToPEM) {
  ByteString der_cert(
      string("01234567890123456789012345678901234567890123456789"), false);
  ByteString expected_pem_cert(
      string(
          "-----BEGIN CERTIFICATE-----\n"
          "MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MDEyMzQ1Njc4OTAxMjM0NTY3\r\n"
          "ODk=\n"
          "-----END CERTIFICATE-----\n"),
      false);
  ByteString pem_cert = Certificates::ConvertDERToPEM(der_cert);
  EXPECT_TRUE(expected_pem_cert.Equals(pem_cert));
}

TEST_F(CertificatesTest, Write) {
  string cert_str = "foo";
  ByteString cert(cert_str, false);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath certfile = temp_dir.path().Append("certfile");
  EXPECT_TRUE(Certificates::Write(cert, certfile));
  string contents;
  EXPECT_TRUE(base::ReadFileToString(certfile, &contents));
  EXPECT_EQ(cert_str, contents);
  struct stat buf;
  EXPECT_EQ(0, stat(certfile.value().c_str(), &buf));
  EXPECT_EQ(S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, buf.st_mode);

  EXPECT_FALSE(Certificates::Write(cert, temp_dir.path().Append("foo/bar")));
}

}  // namespace shims

}  // namespace shill
