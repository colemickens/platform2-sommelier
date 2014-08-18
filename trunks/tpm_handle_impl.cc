// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/tpm_handle_impl.h"

#include <fcntl.h>
#include <unistd.h>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

namespace {

const char kTpmDevice[] = "/dev/tpm0";
const size_t kTpmBufferSize = 4096;
const int kInvalidFileDescriptor = -1;

}  // namespace

namespace trunks {

TpmHandleImpl::TpmHandleImpl(): fd_(kInvalidFileDescriptor) {}

TpmHandleImpl::~TpmHandleImpl() {
  int result = IGNORE_EINTR(close(fd_));
  if (result == -1) {
    PLOG(ERROR) << "TPM: couldn't close " << kTpmDevice;
  }
  LOG(INFO) << "TPM: " << kTpmDevice << " closed successfully";
}

TPM_RC TpmHandleImpl::Init() {
  CHECK_EQ(fd_, kInvalidFileDescriptor);
  fd_ = HANDLE_EINTR(open(kTpmDevice, O_RDWR));
  if (fd_ == kInvalidFileDescriptor) {
    PLOG(ERROR) << "TPM: Error opening tpm0 file descriptor at " << kTpmDevice;
    return TCTI_RC_GENERAL_FAILURE;
  }
  LOG(INFO) << "TPM: " << kTpmDevice << " opened successfully";
  return TPM_RC_SUCCESS;
}


TPM_RC TpmHandleImpl::SendCommand(const std::string command,
                                std::string* response) {
  // TODO(usanghi): Finish implementation and test with a real TPM.
  CHECK_NE(fd_, kInvalidFileDescriptor);
  size_t length = command.length();
  if (length > kTpmBufferSize) {
    LOG(ERROR) << "TPM: command length: " << length
               << " exceeds TPM buffer length: " << kTpmBufferSize;
    return TCTI_RC_INSUFFICIENT_BUFFER;
  }

  int result = HANDLE_EINTR(write(fd_, command.data(), length));
  if (result == -1) {
    PLOG(ERROR) << "TPM: Error writing to TPM Handle";
    return TRUNKS_RC_WRITE_ERROR;
  } else if (static_cast<size_t>(result) != length) {
    LOG(WARNING) << "TPM: Command length: " << length
                 << " was different from length written: " << result;
    return TRUNKS_RC_WRITE_ERROR;
  }

  // TODO(usanghi): reinterpret cast will null terminate, perform
  // explicit and correct memory operations to copy strings.
  uint8_t *response_buf[kTpmBufferSize];
  result = HANDLE_EINTR(read(fd_, response_buf, length));
  if (result == -1) {
    PLOG(ERROR) << "TPM: Error reading from TPM Handle";
    return TRUNKS_RC_READ_ERROR;
  } else if (static_cast<size_t>(result) != length) {
    LOG(WARNING) << "TPM: result length: " << length
                 << " was different from length read: " << result;
    return TRUNKS_RC_READ_ERROR;
  }
  *response = reinterpret_cast<char*>(response_buf);
  return TPM_RC_SUCCESS;
}

}  // namespace trunks
