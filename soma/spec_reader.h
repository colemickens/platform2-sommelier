// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_SPEC_READER_H_
#define SOMA_SPEC_READER_H_

#include <string>

#include <base/files/file_path.h>
#include <base/json/json_reader.h>
#include <base/memory/scoped_ptr.h>

namespace base {
class ListValue;
}

namespace soma {
class ContainerSpec;

// A class that handles reading a container specification written in JSON
// from disk and parsing it into a ContainerSpec object.
class ContainerSpecReader {
 public:
  // Keys for required fields in a container specification.
  static const char kServiceBundlePathKey[];
  static const char kUidKey[];
  static const char kGidKey[];

  // Keys for optional fields in a container specification.
  static const char kDevicePathFiltersKey[];
  static const char kDeviceNodeFiltersKey[];

  ContainerSpecReader();
  virtual ~ContainerSpecReader();

  // Read a container specification at spec_file and return a ContainerSpec
  // object. Returns nullptr on failures and logs appropriate messages.
  scoped_ptr<ContainerSpec> Read(const base::FilePath& spec_file);

 private:
  // Workhorse for doing the parsing of specific fields in the spec.
  scoped_ptr<ContainerSpec> Parse(const std::string& json);

  base::JSONReader reader_;

  DISALLOW_COPY_AND_ASSIGN(ContainerSpecReader);
};
}  // namespace soma

#endif  // SOMA_SPEC_READER_H_
