//
// Copyright (C) 2014 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "trunks/tpm_simulator_handle.h"

#include <fcntl.h>
#include <unistd.h>

#include <base/callback.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>

namespace {

const char kTpmSimRequestFile[] = "/dev/tpm-req";
const char kTpmSimResponseFile[] = "/dev/tpm-resp";
const uint32_t kTpmBufferSize = 4096;
const int kInvalidFileDescriptor = -1;

}  // namespace

namespace trunks {

TpmSimulatorHandle::TpmSimulatorHandle() :
    req_fd_(kInvalidFileDescriptor), resp_fd_(kInvalidFileDescriptor) {}

TpmSimulatorHandle::~TpmSimulatorHandle() {
  int result = IGNORE_EINTR(close(req_fd_));
  if (result == -1) {
    PLOG(ERROR) << "TPM: couldn't close " << kTpmSimRequestFile;
  } else {
    LOG(INFO) << "TPM: " << kTpmSimRequestFile << " closed successfully";
  }
  result = IGNORE_EINTR(close(resp_fd_));
  if (result == -1) {
    PLOG(ERROR) << "TPM: couldn't close " << kTpmSimResponseFile;
  } else {
    LOG(INFO) << "TPM: " << kTpmSimResponseFile << " closed successfully";
  }
}

bool TpmSimulatorHandle::Init() {
  if (req_fd_ == kInvalidFileDescriptor) {
    req_fd_ = HANDLE_EINTR(open("/dev/tpm-req", O_RDWR));
    if (req_fd_ == kInvalidFileDescriptor) {
      PLOG(ERROR) << "TPM: Error opening file descriptor at "
                  << kTpmSimRequestFile;
      return false;
    }
    LOG(INFO) << "TPM: " << kTpmSimRequestFile << " opened successfully";
  }
  if (resp_fd_ == kInvalidFileDescriptor) {
    resp_fd_ = HANDLE_EINTR(open("/dev/tpm-resp", O_RDWR));
    if (resp_fd_ == kInvalidFileDescriptor) {
      PLOG(ERROR) << "TPM: Error opening file descriptor at "
                  << kTpmSimResponseFile;
      return false;
    }
    LOG(INFO) << "TPM: " << kTpmSimResponseFile << " opened successfully";
  }
  return true;
}

void TpmSimulatorHandle::SendCommand(const std::string& command,
                            const ResponseCallback& callback) {
  callback.Run(SendCommandAndWait(command));
}

std::string TpmSimulatorHandle::SendCommandAndWait(const std::string& command) {
  std::string response;
  TPM_RC result = SendCommandInternal(command, &response);
  if (result != TPM_RC_SUCCESS) {
    response = CreateErrorResponse(result);
  }
  return response;
}

TPM_RC TpmSimulatorHandle::SendCommandInternal(const std::string& command,
                                      std::string* response) {
  CHECK_NE(req_fd_, kInvalidFileDescriptor);
  CHECK_NE(resp_fd_, kInvalidFileDescriptor);
  int result = HANDLE_EINTR(write(req_fd_, command.data(), command.length()));
  if (result < 0) {
    PLOG(ERROR) << "TPM: Error writing to TPM simulator request handle.";
    return TRUNKS_RC_WRITE_ERROR;
  }
  if (static_cast<size_t>(result) != command.length()) {
    LOG(ERROR) << "TPM: Error writing to TPM simulator request handle: "
               << result << " vs " << command.length();
    return TRUNKS_RC_WRITE_ERROR;
  }
  char response_buf[kTpmBufferSize];
  result = HANDLE_EINTR(read(resp_fd_, response_buf, kTpmBufferSize));
  if (result < 0) {
    PLOG(ERROR) << "TPM: Error reading from TPM simulator response handle.";
    return TRUNKS_RC_READ_ERROR;
  }
  response->assign(response_buf, static_cast<size_t>(result));
  return TPM_RC_SUCCESS;
}

}  // namespace trunks
