// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_EC_COMMAND_H_
#define BIOD_EC_COMMAND_H_

#include <sys/ioctl.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>

#include <base/logging.h>
#include <base/macros.h>
#include <chromeos/ec/cros_ec_dev.h>

namespace biod {

// Empty request or response for the EcCommand template below.
struct EmptyParam {};
// empty struct is one byte in C++, get the size we want instead.
template <typename T>
constexpr size_t realsizeof() {
  return std::is_empty<T>::value ? 0 : sizeof(T);
}

// Helper to build and send the command structures for cros_fp.
template <typename O, typename I>
class EcCommand {
 public:
  explicit EcCommand(uint32_t cmd, uint32_t ver = 0, const O& req = {})
      : data_({
            .cmd = {.version = ver,
                    .command = cmd,
                    .result = 0xff,
                    .outsize = realsizeof<O>(),
                    .insize = realsizeof<I>()},
            .req = req,
        }) {}
  virtual ~EcCommand() = default;

  void SetRespSize(uint32_t insize) { data_.cmd.insize = insize; }
  void SetReqSize(uint32_t outsize) { data_.cmd.outsize = outsize; }
  void SetReq(const O& req) { data_.req = req; }

  /**
   * Run an EC command. Optionally retry the command when the underlying ioctl
   * returns ETIMEDOUT.
   *
   * @param ec_fd file descriptor for the EC device
   * @param num_attempts number of attempts to try, optional
   * @param result pointer to variable to hold the return code of the command,
   * optional
   * @return true if command runs successfully and response size is same as
   * expected, false otherwise
   *
   * The caller must be careful to only retry EC state-less
   * commands, that can be rerun without consequence.
   */
  bool Run(int ec_fd, int num_attempts = 1, uint16_t* result = nullptr) {
    CHECK_GT(num_attempts, 0);
    for (int retry = 0; retry < num_attempts; retry++) {
      data_.cmd.result = 0xff;
      // We rely on the ioctl preserving data_.req when the command fails.
      // This is important for subsequent retries using the same data_.req.
      int ret = ioctl(ec_fd, CROS_EC_DEV_IOCXCMD_V2, &data_);
      if (ret >= 0) {
        LOG_IF(INFO, retry > 0)
            << "FPMCU ioctl command 0x" << std::hex << data_.cmd.command
            << std::dec << " succeeded on attempt " << retry + 1 << "/"
            << num_attempts << ".";
        if (result != nullptr)
          *result = data_.cmd.result;
        return (static_cast<uint32_t>(ret) == data_.cmd.insize);
      }
      if (result != nullptr)
        // 0xff means Run() failed and we don't have any result.
        *result = 0xff;
      if (errno != ETIMEDOUT) {
        PLOG(ERROR) << "FPMCU ioctl command 0x" << std::hex << data_.cmd.command
                    << std::dec << " failed on attempt " << retry + 1 << "/"
                    << num_attempts << ", retry is not allowed for error";
        return false;
      }
      PLOG(ERROR) << "FPMCU ioctl command 0x" << std::hex << data_.cmd.command
                  << std::dec << " failed on attempt " << retry + 1 << "/"
                  << num_attempts;
    }

    return false;
  }

  I* Resp() { return &data_.resp; }
  uint32_t RespSize() { return data_.cmd.insize; }
  O* Req() { return &data_.req; }
  uint16_t Result() { return data_.cmd.result; }

  struct Data {
    struct cros_ec_command_v2 cmd;
    union {
      O req;
      I resp;
    };
  };

 private:
  Data data_;

  virtual int ioctl(int fd, uint32_t request, Data* data) {
    return ::ioctl(fd, request, data);
  }

  DISALLOW_COPY_AND_ASSIGN(EcCommand);
};

}  // namespace biod

#endif  // BIOD_EC_COMMAND_H_
