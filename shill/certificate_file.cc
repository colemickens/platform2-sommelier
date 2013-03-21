// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/certificate_file.h"

#include <sys/stat.h>

#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <base/stringprintf.h>

#include "shill/glib.h"
#include "shill/logging.h"

using base::FilePath;
using base::SplitString;
using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

const char CertificateFile::kDefaultRootDirectory[] =
    "/var/run/shill/certificate_export";
const char CertificateFile::kPEMHeader[] = "-----BEGIN CERTIFICATE-----";
const char CertificateFile::kPEMFooter[] = "-----END CERTIFICATE-----";

CertificateFile::CertificateFile(GLib *glib)
    : root_directory_(FilePath(kDefaultRootDirectory)),
      glib_(glib) {
  SLOG(Crypto, 2) << __func__;
}

CertificateFile::~CertificateFile() {
  SLOG(Crypto, 2) << __func__;
  if (!output_file_.empty()) {
    file_util::Delete(output_file_, false);
  }
}

FilePath CertificateFile::CreatePEMFromString(const string &pem_contents) {
  string hex_data = ExtractHexData(pem_contents);
  if (hex_data.empty()) {
    return FilePath();
  }
  return WriteFile(StringPrintf(
      "%s\n%s%s\n", kPEMHeader, hex_data.c_str(), kPEMFooter));
}

FilePath CertificateFile::CreateDERFromString(const string &pem_contents) {
  string hex_data = ExtractHexData(pem_contents);
  string der_contents;
  if (!glib_->B64Decode(hex_data, &der_contents)) {
    LOG(ERROR) << "Could not decode hex data from input PEM";
    return FilePath();
  }

  return WriteFile(der_contents);
}

// static
string CertificateFile::ExtractHexData(const std::string &pem_data) {
  bool found_header = false;
  bool found_footer = false;
  const bool kCaseSensitive = false;
  vector<string> input_lines;
  SplitString(pem_data, '\n', &input_lines);
  vector<string> output_lines;
  for (vector<string>::const_iterator it = input_lines.begin();
       it != input_lines.end(); ++it) {
    string line;
    TrimWhitespaceASCII(*it, TRIM_ALL, &line);
    if (StartsWithASCII(line, kPEMHeader, kCaseSensitive)) {
      if (found_header) {
        LOG(ERROR) << "Found two PEM headers in a row.";
        return string();
      } else {
        found_header = true;
        output_lines.clear();
      }
    } else if (StartsWithASCII(line, kPEMFooter, kCaseSensitive)) {
      if (!found_header) {
        LOG(ERROR) << "Found a PEM footer before header.";
        return string();
      } else {
        found_footer = true;
        break;
      }
    } else if (!line.empty()) {
      output_lines.push_back(line);
    }
  }
  if (found_header && !found_footer) {
    LOG(ERROR) << "Found PEM header but no footer.";
    return string();
  }
  DCHECK_EQ(found_header, found_footer);
  output_lines.push_back("");
  return JoinString(output_lines, "\n");
}

FilePath CertificateFile::WriteFile(const string &output_data) {
  if (!file_util::DirectoryExists(root_directory_)) {
    if (!file_util::CreateDirectory(root_directory_)) {
      LOG(ERROR) << "Unable to create parent directory  "
                 << root_directory_.value();
      return FilePath();
    }
    if (chmod(root_directory_.value().c_str(),
              S_IRWXU | S_IXGRP | S_IRGRP | S_IXOTH | S_IROTH)) {
      LOG(ERROR) << "Failed to set permissions on "
                 << root_directory_.value();
      file_util::Delete(root_directory_, true);
      return FilePath();
    }
  }
  if (!output_file_.empty()) {
    file_util::Delete(output_file_, false);
    output_file_ = FilePath();
  }

  FilePath output_file;
  if (!file_util::CreateTemporaryFileInDir(root_directory_, &output_file)) {
    LOG(ERROR) << "Unable to create output file.";
    return FilePath();
  }

  size_t written =
      file_util::WriteFile(output_file, output_data.c_str(),
                           output_data.length());
  if (written != output_data.length()) {
    LOG(ERROR) << "Unable to write to output file.";
    return FilePath();
  }

  if (chmod(output_file.value().c_str(),
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) {
    LOG(ERROR) << "Failed to set permissions on " << output_file.value();
    file_util::Delete(output_file, false);
    return FilePath();
  }
  output_file_ = output_file;
  return output_file_;
}

}  // namespace shill
