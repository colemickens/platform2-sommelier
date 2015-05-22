// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_SANDBOX_SPEC_READER_H_
#define SOMA_LIB_SOMA_SANDBOX_SPEC_READER_H_

#include <memory>

#include <base/files/file_path.h>
#include <soma/soma_export.h>

namespace soma {

class SandboxSpec;

// A class that reads a serialized SandboxSpec protobuffer from disk,
// deserializes it, and returns it.
class SOMA_EXPORT SandboxSpecReader {
 public:
  SandboxSpecReader();
  virtual ~SandboxSpecReader();

  // Read a sandbox specification at spec_file and return a
  // SandboxSpec object. Returns nullptr on failures and logs
  // appropriate messages.
  std::unique_ptr<SandboxSpec> Read(const base::FilePath& spec_file);

 private:
  DISALLOW_COPY_AND_ASSIGN(SandboxSpecReader);
};

}  // namespace soma

#endif  // SOMA_LIB_SOMA_SANDBOX_SPEC_READER_H_
