// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEMFWD_PROTO_FILE_READER_H_
#define MODEMFWD_PROTO_FILE_READER_H_

#include <memory>

#include <base/files/file_path.h>
#include <google/protobuf/message.h>

namespace modemfwd {

bool ReadProtobuf(const base::FilePath& proto_file,
                  google::protobuf::Message* out_proto);

}  // namespace modemfwd

#endif  // MODEMFWD_PROTO_FILE_READER_H_
