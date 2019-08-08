// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BIOD_CROS_FP_DEVICE_H_
#define BIOD_CROS_FP_DEVICE_H_

#include <sys/ioctl.h>

#include <bitset>
#include <memory>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/ec/cros_ec_dev.h>
#include <chromeos/ec/ec_commands.h>

#include "biod/biod_metrics.h"
#include "biod/uinput_device.h"

using MessageLoopForIO = base::MessageLoopForIO;

using VendorTemplate = std::vector<uint8_t>;

namespace biod {

class CrosFpDevice : public MessageLoopForIO::Watcher {
 public:
  using MkbpCallback = base::Callback<void(const uint32_t event)>;
  explicit CrosFpDevice(const MkbpCallback& mkbp_event,
                        BiodMetrics* biod_metrics)
      : mkbp_event_(mkbp_event),
        fd_watcher_(std::make_unique<MessageLoopForIO::FileDescriptorWatcher>(
            FROM_HERE)),
        biod_metrics_(biod_metrics) {}
  ~CrosFpDevice() override;

  struct EcVersion {
    std::string ro_version;
    std::string rw_version;
    ec_current_image current_image = EC_IMAGE_UNKNOWN;
  };

  static std::unique_ptr<CrosFpDevice> Open(const MkbpCallback& callback,
                                            BiodMetrics* biod_metrics);

  // Run a simple command to get the version information from FP MCU and check
  // whether the image type returned is the same as |expected_image|.
  static bool WaitOnEcBoot(const base::ScopedFD& cros_fp_fd,
                           ec_current_image expected_image);

  // Run a simple command to get the version information from FP MCU.
  // The retrieved version is written to |ver|.
  // Returns true if successfully retrieved the version.
  static bool GetVersion(const base::ScopedFD& cros_fp_fd, EcVersion* ver);

  bool FpMode(uint32_t mode);
  bool GetFpMode(uint32_t* mode);
  bool GetFpStats(int* capture_ms, int* matcher_ms, int* overall_ms);
  bool GetDirtyMap(std::bitset<32>* bitmap);
  bool GetTemplate(int index, VendorTemplate* out);
  bool UploadTemplate(const VendorTemplate& tmpl);
  bool SetContext(std::string user_id);
  bool ResetContext();
  // Initialise the entropy in the SBP. If |reset| is true, the old entropy
  // will be deleted. If |reset| is false, we will only add entropy, and only
  // if no entropy had been added before.
  bool InitEntropy(bool reset = false);

  int MaxTemplateCount() { return info_.template_max; }
  int TemplateVersion() { return info_.template_version; }

  // Kernel device exposing the MCU command interface.
  static constexpr char kCrosFpPath[] = "/dev/cros_fp";

  static constexpr int kLastTemplate = -1;

 protected:
  // MessageLoopForIO::Watcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override {}

 private:
  bool Init();

  bool EcDevInit();
  ssize_t ReadVersion(char* buffer, size_t size);
  bool EcProtoInfo(ssize_t* max_read, ssize_t* max_write);
  bool EcReboot(ec_current_image to_image);
  // Run the EC command to generate new entropy in the underlying MCU.
  // |reset| specifies whether we want to merely add entropy (false), or
  // perform a reset, which erases old entropy(true).
  bool AddEntropy(bool reset);
  // Get block id from rollback info.
  bool GetRollBackInfoId(int32_t* block_id);
  bool FpFrame(int index, std::vector<uint8_t>* frame);
  bool UpdateFpInfo();
  // Run a sequence of EC commands to update the entropy in the
  // MCU. If |reset| is set to true, it will additionally erase the existing
  // entropy too.
  bool UpdateEntropy(bool reset);

  base::ScopedFD cros_fd_;
  ssize_t max_read_size_ = 0;
  ssize_t max_write_size_ = 0;
  struct ec_response_fp_info info_ = {};

  MkbpCallback mkbp_event_;
  UinputDevice input_device_;

  std::unique_ptr<MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  BiodMetrics* biod_metrics_ = nullptr;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(CrosFpDevice);
};

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
  O* Req() { return &data_.req; }
  uint16_t Result() { return data_.cmd.result; }

 private:
  struct {
    struct cros_ec_command_v2 cmd;
    union {
      O req;
      I resp;
    };
  } data_;

  DISALLOW_COPY_AND_ASSIGN(EcCommand);
};

}  // namespace biod

#endif  // BIOD_CROS_FP_DEVICE_H_
