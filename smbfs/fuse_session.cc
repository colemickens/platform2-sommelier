// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbfs/fuse_session.h"

#include <errno.h>
#include <unistd.h>

#include <utility>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/posix/safe_strerror.h>

#include "smbfs/filesystem.h"
#include "smbfs/request.h"

namespace smbfs {

FuseSession::FuseSession(std::unique_ptr<Filesystem> fs, fuse_chan* chan)
    : fs_(std::move(fs)), chan_(chan) {
  DCHECK(fs_);
  DCHECK(chan_);
}

FuseSession::~FuseSession() {
  // Ensure |stop_callback_| isn't called as a result of destruction.
  stop_callback_ = {};

  if (session_) {
    // Disconnect the channel from the fuse_session before destroying them both.
    // fuse_session_destroy() also destroys the attached channel (not
    // documented). Disconnecting the two simplifies logic, and ensures
    // FuseSession maintains ownership of the fuse_chan.
    fuse_session_remove_chan(chan_);
    fuse_session_destroy(session_);
  }
  fuse_chan_destroy(chan_);
}

bool FuseSession::Start(base::OnceClosure stop_callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  CHECK_EQ(session_, nullptr);

  fuse_lowlevel_ops ops = {0};
  ops.destroy = &FuseSession::FuseDestroy;
  ops.lookup = &FuseSession::FuseLookup;
  ops.getattr = &FuseSession::FuseGetAttr;

  session_ = fuse_lowlevel_new(nullptr, &ops, sizeof(ops), this);
  if (!session_) {
    LOG(ERROR) << "Unable to create new FUSE session";
    return false;
  }
  fuse_session_add_chan(session_, chan_);
  read_buffer_.resize(fuse_chan_bufsize(chan_));

  CHECK(base::SetNonBlocking(fuse_chan_fd(chan_)));
  read_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      fuse_chan_fd(chan_), base::BindRepeating(&FuseSession::OnChannelReadable,
                                               base::Unretained(this)));
  stop_callback_ = std::move(stop_callback);

  return true;
}

// static
void FuseSession::FuseDestroy(void* userdata) {
  static_cast<FuseSession*>(userdata)->Destroy();
}

// static
void FuseSession::FuseLookup(fuse_req_t request,
                             fuse_ino_t parent_inode,
                             const char* name) {
  static_cast<FuseSession*>(fuse_req_userdata(request))
      ->Lookup(request, parent_inode, name);
}

// static
void FuseSession::FuseGetAttr(fuse_req_t request,
                              fuse_ino_t inode,
                              fuse_file_info* info) {
  static_cast<FuseSession*>(fuse_req_userdata(request))
      ->GetAttr(request, inode, info);
}

void FuseSession::Destroy() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  VLOG(1) << "FuseSession::Destroy";
  RequestStop();
}

void FuseSession::Lookup(fuse_req_t request,
                         fuse_ino_t parent_inode,
                         const char* name) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  VLOG(1) << "FuseSession::Lookup parent: " << parent_inode << " name:" << name;
  fs_->Lookup(std::make_unique<EntryRequest>(request), parent_inode, name);
}

void FuseSession::GetAttr(fuse_req_t request,
                          fuse_ino_t inode,
                          fuse_file_info* unused_info) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  VLOG(1) << "FuseSession::GetAttr: " << inode;
  fs_->GetAttr(std::make_unique<AttrRequest>(request), inode);
}

void FuseSession::OnChannelReadable() {
  VLOG(2) << "FuseSession::OnChannelReadable";
  fuse_buf buf = {
      .mem = read_buffer_.data(),
      .size = read_buffer_.size(),
  };
  fuse_chan* temp_chan = chan_;
  int read_size = fuse_session_receive_buf(session_, &buf, &temp_chan);
  if (read_size == 0) {
    // A read of 0 indicates the filesystem has been unmounted and the kernel
    // driver has closed the fuse session.
    LOG(INFO) << "FUSE kernel channel closed, shutting down";
    RequestStop();
    return;
  } else if (read_size == -EINTR) {
    // Since FD watching is level-triggered, this function will get called again
    // very soon to try again.
    return;
  } else if (read_size < 0) {
    LOG(ERROR) << "FUSE channel read failed with error: "
               << base::safe_strerror(-read_size) << " [" << -read_size
               << "], shutting down";
    RequestStop();
    return;
  }

  fuse_session_process_buf(session_, &buf, chan_);
}

void FuseSession::RequestStop() {
  read_watcher_.reset();

  if (stop_callback_) {
    std::move(stop_callback_).Run();
  }
}

}  // namespace smbfs
