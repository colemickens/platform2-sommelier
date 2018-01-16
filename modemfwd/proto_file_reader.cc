// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/proto_file_reader.h"

#include <utility>

#include <base/files/file.h>
#include <base/macros.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace {

class FileInputStream : public google::protobuf::io::CopyingInputStream {
 public:
  explicit FileInputStream(base::File file) : file_(std::move(file)) {}
  ~FileInputStream() override = default;

  // CopyingInputStream overrides.
  int Read(void* buffer, int size) override {
    return file_.ReadAtCurrentPos(static_cast<char*>(buffer), size);
  }

 private:
  base::File file_;

  DISALLOW_COPY_AND_ASSIGN(FileInputStream);
};

}  // namespace

namespace modemfwd {

bool ReadProtobuf(const base::FilePath& proto_file,
                  google::protobuf::MessageLite* out_proto) {
  DCHECK(out_proto);

  base::File file(proto_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    DLOG(ERROR) << "Could not open \"" << proto_file.value()
                << "\": " << base::File::ErrorToString(file.error_details());
    return false;
  }

  FileInputStream input_stream(std::move(file));
  google::protobuf::io::CopyingInputStreamAdaptor adaptor(&input_stream);
  return out_proto->ParseFromZeroCopyStream(&adaptor);
}

}  // namespace modemfwd
