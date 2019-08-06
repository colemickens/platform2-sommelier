// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbfs/request.h"

#include <errno.h>

#include <base/logging.h>

namespace smbfs {
namespace internal {

BaseRequest::BaseRequest(fuse_req_t req) : req_(req) {}

BaseRequest::~BaseRequest() {
  if (!replied_) {
    // If a reply was not sent, either because the request was interrupted or
    // the filesystem is being shut down, send an error reply so that the
    // request can be freed.
    fuse_reply_err(req_, EINTR);
  }
}

void BaseRequest::ReplyError(int error) {
  DCHECK(!replied_);
  DCHECK_GT(error, 0);

  fuse_reply_err(req_, error);
  replied_ = true;
}

}  // namespace internal

AttrRequest::AttrRequest(fuse_req_t req) : internal::BaseRequest(req) {}

void AttrRequest::ReplyAttr(const struct stat* attr, double attr_timeout) {
  DCHECK(!replied_);
  DCHECK(attr);

  fuse_reply_attr(req_, attr, attr_timeout);
  replied_ = true;
}

EntryRequest::EntryRequest(fuse_req_t req) : internal::BaseRequest(req) {}

void EntryRequest::ReplyEntry(const fuse_entry_param* entry) {
  DCHECK(!replied_);
  DCHECK(entry);

  fuse_reply_entry(req_, entry);
  replied_ = true;
}

}  // namespace smbfs
