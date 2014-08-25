// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trunks/tpm_handle_impl.h"

#include <fcntl.h>
#include <unistd.h>

#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/sys_byteorder.h>

namespace {

const char kTpmDevice[] = "/dev/tpm0";
const uint32_t kTpmBufferSize = 4096;
const int kInvalidFileDescriptor = -1;
const uint32_t kTpmHeaderLength = 10;
const uint32_t kTpmHeaderLengthIndex = 2;

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

TPM_RC TpmHandleImpl::SendCommand(const std::string& command,
                                  std::string* response) {
  CHECK_NE(fd_, kInvalidFileDescriptor);
  TPM_RC command_verify = VerifyCommand(command);
  if (command_verify != TPM_RC_SUCCESS) {
    return command_verify;
  }

  int result = HANDLE_EINTR(write(fd_, command.data(), command.length()));
  if (result < 0 || static_cast<uint32_t>(result) < kTpmHeaderLength) {
    PLOG(ERROR) << "TPM: Error writing to TPM Handle";
    return TRUNKS_RC_WRITE_ERROR;
  }
  char response_buf[kTpmBufferSize];
  result = HANDLE_EINTR(read(fd_, response_buf, kTpmBufferSize));
  if (result < 0 || static_cast<uint32_t>(result) < kTpmHeaderLength) {
    PLOG(ERROR) << "TPM: Error reading from TPM Handle";
    return TRUNKS_RC_READ_ERROR;
  }

  response->assign(response_buf, static_cast<size_t>(result));
  return TPM_RC_SUCCESS;
}

TPM_RC TpmHandleImpl::VerifyCommand(const std::string& command) {
  uint32_t length = command.length();
  if (length > kTpmBufferSize) {
    LOG(ERROR) << "TPM: command length: " << length
               << " exceeds TPM buffer length: " << kTpmBufferSize;
    return TCTI_RC_INSUFFICIENT_BUFFER;
  }
  if (length < kTpmHeaderLength) {
    LOG(ERROR) << "TPM: command length " << length
               << " is smaller than TPM header length.";
    return TCTI_RC_BAD_PARAMETER;
  }
  size_t command_length = GetMessageLength(command.data());
  if (command_length != length) {
    LOG(ERROR) << "TPM: length to transmit is: " << length
               << " but tpm_header says length is: " << command_length;
    return TCTI_RC_BAD_PARAMETER;
  }
  VLOG(1) << "TPM: Command successfully verified.";
  return TPM_RC_SUCCESS;
}

uint32_t TpmHandleImpl::GetMessageLength(const char* tpm_header) {
  uint32_t value = 0;
  memcpy(&value, &tpm_header[kTpmHeaderLengthIndex], sizeof(uint32_t));
  return base::NetToHost32(value);
}

}  // namespace trunks
