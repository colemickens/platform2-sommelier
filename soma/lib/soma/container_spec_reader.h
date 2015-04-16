// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_CONTAINER_SPEC_READER_H_
#define SOMA_LIB_SOMA_CONTAINER_SPEC_READER_H_

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/json/json_reader.h>
#include <soma/isolator_parser.h>
#include <soma/soma_export.h>

namespace base {
class ListValue;
}

namespace soma {

class ContainerSpec;

namespace parser {

class UserdbInterface;

// A class that handles reading a container specification written in JSON
// from disk and parsing it into a ContainerSpecWrapper object.
class SOMA_EXPORT ContainerSpecReader {
 public:
  static const char kServiceBundleRoot[];

  // Keys for required fields in a container specification.
  static const char kServiceBundleNameKey[];
  static const char kAppsListKey[];
  static const char kSubAppKey[];
  // These keys are nested beneath kSubAppKey.
  static const char kCommandLineKey[];
  static const char kGidKey[];
  static const char kUidKey[];

  // Keys for optional fields in a container specification.
  static const char kIsolatorsListKey[];

  ContainerSpecReader();
  explicit ContainerSpecReader(std::unique_ptr<UserdbInterface> userdb);
  virtual ~ContainerSpecReader();

  // Read a container specification at spec_file and return a
  // ContainerSpec object. Returns nullptr on failures and logs
  // appropriate messages.
  std::unique_ptr<ContainerSpec> Read(const base::FilePath& spec_file);

 private:
  bool ParseIsolators(const base::ListValue& isolators, ContainerSpec* spec);

  IsolatorParserMap isolator_parsers_;
  base::JSONReader reader_;
  std::unique_ptr<UserdbInterface> userdb_;

  DISALLOW_COPY_AND_ASSIGN(ContainerSpecReader);
};
}  // namespace parser
}  // namespace soma

#endif  // SOMA_LIB_SOMA_CONTAINER_SPEC_READER_H_
