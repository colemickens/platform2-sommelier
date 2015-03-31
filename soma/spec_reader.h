// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_SPEC_READER_H_
#define SOMA_SPEC_READER_H_

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/json/json_reader.h>

namespace base {
class ListValue;
}

namespace soma {
class ContainerSpecWrapper;

namespace parser {
// A class that handles reading a container specification written in JSON
// from disk and parsing it into a ContainerSpecWrapper object.
class ContainerSpecReader {
 public:
  static const char kServiceBundleRoot[];

  // Keys for required fields in a container specification.
  static const char kServiceBundleNameKey[];
  static const char kAppsKey[];
  static const char kCommandLineKey[];
  static const char kGidKey[];
  static const char kUidKey[];

  ContainerSpecReader();
  virtual ~ContainerSpecReader();

  // Read a container specification at spec_file and return a
  // ContainerSpecWrapper object. Returns nullptr on failures and logs
  // appropriate messages.
  std::unique_ptr<ContainerSpecWrapper> Read(const base::FilePath& spec_file);

 private:
  // Workhorse for doing the parsing of specific fields in the spec.
  std::unique_ptr<ContainerSpecWrapper> Parse(const std::string& json);

  base::JSONReader reader_;

  DISALLOW_COPY_AND_ASSIGN(ContainerSpecReader);
};
}  // namespace parser
}  // namespace soma

#endif  // SOMA_SPEC_READER_H_
