// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBFS_REQUEST_H_
#define SMBFS_REQUEST_H_

#include <fuse_lowlevel.h>

#include <base/macros.h>

namespace smbfs {
namespace internal {

// Base class for maintaining state about a fuse request, and ensuring requests
// are responded to correctly.
class BaseRequest {
 public:
  void ReplyError(int error);

 protected:
  explicit BaseRequest(fuse_req_t req);
  ~BaseRequest();

  const fuse_req_t req_;
  bool replied_ = false;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BaseRequest);
};

}  // namespace internal

// State of fuse requests that can be responsed to with an attributes response.
class AttrRequest : public internal::BaseRequest {
 public:
  explicit AttrRequest(fuse_req_t req);
  void ReplyAttr(const struct stat* attr, double attr_timeout);
};

// State of fuse requests that can be responsed to with an entry response.
class EntryRequest : public internal::BaseRequest {
 public:
  explicit EntryRequest(fuse_req_t req);
  void ReplyEntry(const fuse_entry_param* entry);
};

}  // namespace smbfs

#endif  // SMBFS_REQUEST_H_
