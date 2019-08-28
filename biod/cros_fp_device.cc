// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/cros_fp_device.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <algorithm>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>

namespace {

// Upper bound of the host command packet transfer size.
constexpr int kMaxPacketSize = 544;
// Although very rare, we have seen device commands fail due
// to ETIMEDOUT. For this reason, we attempt certain critical
// device IO operation twice.
constexpr int kMaxIoAttempts = 2;

std::string FourCC(const uint32_t a) {
  return base::StringPrintf(
      "%c%c%c%c", static_cast<char>(a), static_cast<char>(a >> 8),
      static_cast<char>(a >> 16), static_cast<char>(a >> 24));
}

}  // namespace

namespace biod {

constexpr char CrosFpDevice::kCrosFpPath[];

CrosFpDevice::~CrosFpDevice() {
  // Current session is gone, clean-up temporary state in the FP MCU.
  if (cros_fd_.is_valid())
    ResetContext();
}

std::unique_ptr<CrosFpDevice> CrosFpDevice::Open(const MkbpCallback& callback,
                                                 BiodMetrics* biod_metrics) {
  auto dev = std::make_unique<CrosFpDevice>(callback, biod_metrics);
  if (!dev->Init()) {
    return nullptr;
  }
  return dev;
}

bool CrosFpDevice::EcProtoInfo(ssize_t* max_read, ssize_t* max_write) {
  /* read max request / response size from the MCU for protocol v3+ */
  EcCommand<EmptyParam, struct ec_response_get_protocol_info> cmd(
      EC_CMD_GET_PROTOCOL_INFO);
  // We retry this command because it is known to occasionally fail
  // with ETIMEDOUT on first attempt.
  if (!cmd.Run(cros_fd_.get(), kMaxIoAttempts))
    return false;

  *max_read =
      cmd.Resp()->max_response_packet_size - sizeof(struct ec_host_response);
  // TODO(vpalatin): workaround for b/78544921, can be removed if MCU is fixed.
  *max_write =
      cmd.Resp()->max_request_packet_size - sizeof(struct ec_host_request) - 4;
  return true;
}

ssize_t CrosFpDevice::ReadVersion(char* buffer, size_t size) {
  ssize_t ret;
  for (int retry = 0; retry < kMaxIoAttempts; retry++) {
    ret = read(cros_fd_.get(), buffer, size);
    if (ret >= 0) {
      LOG_IF(INFO, retry > 0)
          << "FPMCU read cros_fp device succeeded on attempt " << retry + 1
          << "/" << kMaxIoAttempts << ".";
      return ret;
    }
    if (errno != ETIMEDOUT) {
      PLOG(ERROR) << "FPMCU failed to read cros_fp device on attempt "
                  << retry + 1 << "/" << kMaxIoAttempts
                  << ", retry is not allowed for error";
      return ret;
    }
    PLOG(ERROR) << "FPMCU failed to read cros_fp device on attempt "
                << retry + 1 << "/" << kMaxIoAttempts;
  }

  return ret;
}

bool CrosFpDevice::EcDevInit() {
  // This is a special read (before events are enabled) that can fail due
  // to ETIMEDOUT. This is because the first read with events disabled
  // triggers a get_version request to the FPMCU, which can timeout.
  // TODO(b/131438292): Remove the hardcoded size for the version buffer.
  char version[80];
  ssize_t ret = ReadVersion(version, sizeof(version) - 1);
  if (ret <= 0) {
    LOG(ERROR) << "Failed to read cros_fp device version, read returned " << ret
               << ".";
    return false;
  }
  version[ret] = '\0';
  LOG(INFO) << "cros_fp device version: " << version;
  char* s = strchr(version, '\n');
  if (s)
    *s = '\0';
  if (strcmp(version, CROS_EC_DEV_VERSION)) {
    LOG(ERROR) << "Invalid device version";
    return false;
  }

  if (!EcProtoInfo(&max_read_size_, &max_write_size_)) {
    LOG(ERROR) << "Failed to get cros_fp protocol info.";
    return false;
  }

  unsigned long mask = 1 << EC_MKBP_EVENT_FINGERPRINT;  // NOLINT(runtime/int)
  if (ioctl(cros_fd_.get(), CROS_EC_DEV_IOCEVENTMASK_V2, mask) < 0) {
    LOG(ERROR) << "Fail to request fingerprint events";
    return false;
  }

  return true;
}

void CrosFpDevice::OnEventReadable() {
  struct ec_response_get_next_event evt;
  ssize_t sz = read(cros_fd_.get(), &evt, sizeof(evt));

  // We are interested only in fingerprint events, discard the other ones.
  if (evt.event_type != EC_MKBP_EVENT_FINGERPRINT ||
      sz < sizeof(evt.event_type) + sizeof(evt.data.fp_events))
    return;

  // Properly aligned event value.
  uint32_t events;
  memcpy(&events, &evt.data.fp_events, sizeof(events));
  mkbp_event_.Run(events);
}

bool CrosFpDevice::SetFpMode(const FpMode& mode) {
  EcCommand<struct ec_params_fp_mode, struct ec_response_fp_mode> cmd(
      EC_CMD_FP_MODE, 0, {.mode = mode.RawVal()});
  bool ret = cmd.Run(cros_fd_.get());
  if (ret) {
    return true;
  }

  // In some cases the EC Command might go through, but the AP suspends
  // before the EC can ACK it. When the AP wakes up, it considers the
  // EC command to have timed out. Since this seems to happen during mode
  // setting, check the mode in case of a failure.
  FpMode cur_mode;
  if (!GetFpMode(&cur_mode)) {
    LOG(ERROR) << "Failed to get FP mode to verify mode was set in the MCU.";
    return false;
  }
  if (cur_mode == mode) {
    LOG(WARNING)
        << "EC Command to set mode failed, but mode was set successfully.";
    return true;
  } else {
    LOG(ERROR) << "EC command to set FP mode: " << mode
               << " failed; current FP mode: " << cur_mode;
  }
  return false;
}

bool CrosFpDevice::GetFpMode(FpMode* mode) {
  EcCommand<struct ec_params_fp_mode, struct ec_response_fp_mode> cmd(
      EC_CMD_FP_MODE, 0, {.mode = static_cast<uint32_t>(FP_MODE_DONT_CHANGE)});
  if (!cmd.Run(cros_fd_.get())) {
    LOG(ERROR) << "Failed to get FP mode from MCU.";
    return false;
  }

  *mode = FpMode(cmd.Resp()->mode);
  return true;
}

bool CrosFpDevice::FpFrame(int index, std::vector<uint8_t>* frame) {
  EcCommand<struct ec_params_fp_frame, uint8_t[kMaxPacketSize]> cmd(
      EC_CMD_FP_FRAME);

  uint32_t offset = index << FP_FRAME_INDEX_SHIFT;
  uint8_t* payload = cmd.Resp()[0];
  auto pos = frame->begin();
  while (pos < frame->end()) {
    uint32_t len = std::min(max_read_size_, frame->end() - pos);
    cmd.SetReq({.offset = offset, .size = len});
    cmd.SetRespSize(len);
    int retries = 0;
    const int max_retries = 50, delay_ms = 100;
    while (!cmd.Run(cros_fd_.get())) {
      if (!(offset & FP_FRAME_OFFSET_MASK)) {
        // On the first request, the EC might still be rate-limiting. Retry in
        // that case.
        if (cmd.Result() == EC_RES_BUSY && retries < max_retries) {
          retries++;
          LOG(INFO) << "Retrying FP_FRAME, attempt " << retries;
          base::PlatformThread::Sleep(
              base::TimeDelta::FromMilliseconds(delay_ms));
          continue;
        }
      }
      LOG(ERROR) << "FP_FRAME command failed @ 0x" << std::hex << offset;
      return false;
    }
    std::copy(payload, payload + len, pos);
    offset += len;
    pos += len;
  }
  return true;
}

bool CrosFpDevice::UpdateFpInfo() {
  EcCommand<EmptyParam, struct ec_response_fp_info> cmd(EC_CMD_FP_INFO, 1);
  if (!cmd.Run(cros_fd_.get())) {
    LOG(ERROR) << "Failed to get FP information";
    return false;
  }
  info_ = *cmd.Resp();
  return true;
}

bool CrosFpDevice::GetFpStats(int* capture_ms,
                              int* matcher_ms,
                              int* overall_ms) {
  EcCommand<EmptyParam, struct ec_response_fp_stats> cmd(EC_CMD_FP_STATS);
  if (!cmd.Run(cros_fd_.get()))
    return false;

  uint8_t inval = cmd.Resp()->timestamps_invalid;
  if (inval & (FPSTATS_CAPTURE_INV | FPSTATS_MATCHING_INV))
    return false;

  *capture_ms = cmd.Resp()->capture_time_us / 1000;
  *matcher_ms = cmd.Resp()->matching_time_us / 1000;
  *overall_ms = cmd.Resp()->overall_time_us / 1000;

  return true;
}

// static
bool CrosFpDevice::WaitOnEcBoot(const base::ScopedFD& cros_fp_fd,
                                ec_current_image expected_image) {
  int tries = 50;
  ec_current_image image = EC_IMAGE_UNKNOWN;

  while (tries) {
    tries--;
    // Check the EC has the right image.
    EcCommand<EmptyParam, struct ec_response_get_version> cmd(
        EC_CMD_GET_VERSION);
    if (!cmd.Run(cros_fp_fd.get())) {
      LOG(ERROR) << "Failed to retrieve cros_fp firmware version.";
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(500));
      continue;
    }
    image = static_cast<ec_current_image>(cmd.Resp()->current_image);
    if (image == expected_image) {
      LOG(INFO) << "EC image is " << (image == EC_IMAGE_RO ? "RO" : "RW")
                << ".";
      return true;
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }
  LOG(ERROR) << "EC rebooted to incorrect image " << image;
  return false;
}

// static
bool CrosFpDevice::GetVersion(const base::ScopedFD& cros_fp_fd,
                              EcVersion* ver) {
  CHECK(ver);

  EcCommand<EmptyParam, struct ec_response_get_version> cmd(EC_CMD_GET_VERSION);
  if (!cmd.Run(cros_fp_fd.get())) {
    LOG(ERROR) << "Failed to fetch cros_fp firmware version.";
    return false;
  }

  // buffers should already be null terminated -- this is a safeguard
  cmd.Resp()->version_string_ro[sizeof(cmd.Resp()->version_string_ro) - 1] =
      '\0';
  cmd.Resp()->version_string_rw[sizeof(cmd.Resp()->version_string_rw) - 1] =
      '\0';

  ver->ro_version = std::string(cmd.Resp()->version_string_ro);
  ver->rw_version = std::string(cmd.Resp()->version_string_rw);
  ver->current_image = static_cast<ec_current_image>(cmd.Resp()->current_image);
  return true;
}

bool CrosFpDevice::EcReboot(ec_current_image to_image) {
  DCHECK(to_image == EC_IMAGE_RO || to_image == EC_IMAGE_RW);

  EcCommand<EmptyParam, EmptyParam> cmd_reboot(EC_CMD_REBOOT);
  // Don't expect a return code, cros_fp has rebooted.
  cmd_reboot.Run(cros_fd_.get());

  if (!WaitOnEcBoot(cros_fd_, EC_IMAGE_RO)) {
    LOG(ERROR) << "EC did not come back up after reboot.";
    return false;
  }

  if (to_image == EC_IMAGE_RO) {
    // Tell the EC to remain in RO.
    EcCommand<struct ec_params_rwsig_action, EmptyParam> cmd_rwsig(
        EC_CMD_RWSIG_ACTION);
    cmd_rwsig.SetReq({.action = RWSIG_ACTION_ABORT});
    if (!cmd_rwsig.Run(cros_fd_.get())) {
      LOG(ERROR) << "Failed to keep cros_fp in RO.";
      return false;
    }
  }

  // EC jumps to RW after 1 second. Wait enough time in case we want to reboot
  // to RW. In case we wanted to remain in RO, wait anyway to ensure that the EC
  // received the instructions.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(3));

  if (!WaitOnEcBoot(cros_fd_, to_image)) {
    LOG(ERROR) << "EC did not load the right image.";
    return false;
  }

  return true;
}

bool CrosFpDevice::AddEntropy(bool reset) {
  // Create the secret.
  EcCommand<struct ec_params_rollback_add_entropy, EmptyParam> cmd_add_entropy(
      EC_CMD_ADD_ENTROPY);
  if (reset) {
    cmd_add_entropy.SetReq({.action = ADD_ENTROPY_RESET_ASYNC});
  } else {
    cmd_add_entropy.SetReq({.action = ADD_ENTROPY_ASYNC});
  }
  if (!cmd_add_entropy.Run(cros_fd_.get())) {
    LOG(ERROR) << "Failed to send command to add entropy.";
    return false;
  }
  int tries = 20;
  while (tries) {
    tries--;
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
    cmd_add_entropy.SetReq({.action = ADD_ENTROPY_GET_RESULT});
    // EC will return EC_RES_BUSY initially, ignore the return code.
    cmd_add_entropy.Run(cros_fd_.get());
    if (cmd_add_entropy.Result() == EC_RES_SUCCESS) {
      LOG(INFO) << "Entropy has been successfully added.";
      return true;
    }
  }
  LOG(ERROR) << "Failed to check status of entropy command.";
  return false;
}

bool CrosFpDevice::GetRollBackInfoId(int32_t* block_id) {
  EcCommand<EmptyParam, struct ec_response_rollback_info> cmd_rb_info(
      EC_CMD_ROLLBACK_INFO);
  if (!cmd_rb_info.Run(cros_fd_.get())) {
    return false;
  }

  *block_id = cmd_rb_info.Resp()->id;
  return true;
}

bool CrosFpDevice::InitEntropy(bool reset) {
  int32_t block_id;
  if (!GetRollBackInfoId(&block_id)) {
    LOG(ERROR) << "Failed to read block ID from FPMCU.";
    return false;
  }

  if (!reset && block_id != 0) {
    // Secret has been set.
    LOG(INFO) << "Entropy source had been initialized previously.";
    return true;
  }
  LOG(INFO) << "Entropy source has not been initialized yet.";

  bool success = UpdateEntropy(reset);
  if (!success) {
    LOG(INFO) << "Entropy addition failed.";
    return false;
  }
  LOG(INFO) << "Entropy has been successfully added.";
  return true;
}

bool CrosFpDevice::Init() {
  cros_fd_ = base::ScopedFD(open(kCrosFpPath, O_RDWR));
  if (cros_fd_.get() < 0) {
    LOG(ERROR) << "Failed to open " << kCrosFpPath;
    return false;
  }

  if (!EcDevInit())
    return false;

  if (!InitEntropy(false)) {
    return false;
  }

  // Clean MCU memory if anything is remaining from aborted sessions
  ResetContext();

  // Retrieve the sensor information / parameters.
  if (!UpdateFpInfo())
    return false;

  LOG(INFO) << "CROS FP Sensor Info ";
  LOG(INFO) << "  Vendor ID  : " << FourCC(info_.vendor_id);
  LOG(INFO) << "  Product ID : " << info_.product_id;
  LOG(INFO) << "  Model ID   : 0x" << std::hex << info_.model_id;
  LOG(INFO) << "  Version    : " << info_.version;
  std::string error_flags;
  if (info_.errors & FP_ERROR_NO_IRQ)
    error_flags += "NO_IRQ ";
  if (info_.errors & FP_ERROR_SPI_COMM)
    error_flags += "SPI_COMM ";
  if (info_.errors & FP_ERROR_BAD_HWID)
    error_flags += "BAD_HWID ";
  if (info_.errors & FP_ERROR_INIT_FAIL)
    error_flags += "INIT_FAIL";
  LOG(INFO) << "  Errors     : " << error_flags;
  LOG(INFO) << "CROS FP Image Info ";
  // Prints the pixel format in FOURCC format.
  LOG(INFO) << "  Pixel Format     : " << FourCC(info_.pixel_format);
  LOG(INFO) << "  Image Data Size  : " << info_.frame_size;
  LOG(INFO) << "  Image Dimensions : " << info_.width << "x" << info_.height
            << " " << info_.bpp << " bpp";
  LOG(INFO) << "CROS FP Finger Template Info ";
  LOG(INFO) << "  Template data format  : " << info_.template_version;
  LOG(INFO) << "  Template Data Size    : " << info_.template_size;
  LOG(INFO) << "  Max number of fingers : " << info_.template_max;

  watcher_ = base::FileDescriptorWatcher::WatchReadable(
      cros_fd_.get(), base::BindRepeating(&CrosFpDevice::OnEventReadable,
                                          base::Unretained(this)));
  if (!watcher_) {
    LOG(ERROR) << "Unable to watch MKBP events";
    return false;
  }

  if (!input_device_.Init()) {
    LOG(ERROR) << "Failed to create Uinput device";
    return false;
  }

  return true;
}

bool CrosFpDevice::GetDirtyMap(std::bitset<32>* bitmap) {
  // Retrieve the up-to-date dirty bitmap from the MCU.
  if (!UpdateFpInfo())
    return false;

  *bitmap = std::bitset<32>(info_.template_dirty);
  return true;
}

bool CrosFpDevice::GetTemplate(int index, VendorTemplate* out) {
  if (index == kLastTemplate) {
    // Get the count of valid templates and the dirty bitmap.
    if (!UpdateFpInfo())
      return false;
    // Use the last template.
    index = info_.template_valid - 1;
    // Is the last one really a new created one ?
    if (!(info_.template_dirty & (1 << index)))
      return false;
  }
  out->resize(static_cast<size_t>(info_.template_size));
  // In the EC_CMD_FP_FRAME host command, the templates are indexed starting
  // from 1 (aka FP_FRAME_INDEX_TEMPLATE), as 0 (aka FP_FRAME_INDEX_RAW_IMAGE)
  // is used for the finger image.
  return FpFrame(index + FP_FRAME_INDEX_TEMPLATE, out);
}

bool CrosFpDevice::UploadTemplate(const VendorTemplate& tmpl) {
  union cmd_with_data {
    struct ec_params_fp_template req;
    uint8_t _fullsize[kMaxPacketSize];
  };
  EcCommand<union cmd_with_data, EmptyParam> cmd(EC_CMD_FP_TEMPLATE);
  struct ec_params_fp_template* req = &cmd.Req()->req;

  size_t max_chunk =
      max_write_size_ - offsetof(struct ec_params_fp_template, data);

  auto pos = tmpl.begin();
  while (pos < tmpl.end()) {
    size_t remaining = tmpl.end() - pos;
    uint32_t tlen = std::min(max_chunk, remaining);
    req->offset = pos - tmpl.begin();
    req->size = tlen | (remaining == tlen ? FP_TEMPLATE_COMMIT : 0);
    std::copy(pos, pos + tlen, req->data);
    cmd.SetReqSize(tlen + sizeof(struct ec_params_fp_template));
    if (!cmd.Run(cros_fd_.get()) || cmd.Result() != EC_RES_SUCCESS) {
      LOG(ERROR) << "FP_TEMPLATE command failed @ " << pos - tmpl.begin();
      return false;
    }
    pos += tlen;
  }
  return true;
}

bool CrosFpDevice::SetContext(std::string user_hex) {
  struct ec_params_fp_context ctxt = {};
  if (!user_hex.empty()) {
    std::vector<uint8_t> user_id;
    if (base::HexStringToBytes(user_hex, &user_id))
      memcpy(ctxt.userid, user_id.data(),
             std::min(user_id.size(), sizeof(ctxt.userid)));
  }
  EcCommand<struct ec_params_fp_context, EmptyParam> cmd(EC_CMD_FP_CONTEXT, 0,
                                                         ctxt);
  return cmd.Run(cros_fd_.get());
}

bool CrosFpDevice::ResetContext() {
  FpMode cur_mode;
  if (!GetFpMode(&cur_mode)) {
    LOG(ERROR) << "Unable to get FP Mode.";
  }

  // ResetContext is called when we no longer expect any session to be running
  // (such as when the user logs out or biod is starting/stopping). This check
  // exists to make sure that we have disabled any matching in the firmware
  // when this is called. See https://crbug.com/980614 for details.
  if (cur_mode != FpMode(FpMode::Mode::kNone)) {
    LOG(ERROR) << "Attempting to reset context with mode: " << cur_mode;
  }

  biod_metrics_->SendResetContextMode(cur_mode);

  return SetContext(std::string());
}

bool CrosFpDevice::UpdateEntropy(bool reset) {
  // Stash the most recent block id.
  int32_t block_id;
  if (!GetRollBackInfoId(&block_id)) {
    LOG(ERROR) << "Failed to block ID from FPMCU before entropy reset.";
    return false;
  }

  // Reboot the EC to RO.
  if (!EcReboot(EC_IMAGE_RO)) {
    LOG(ERROR) << "Failed to reboot cros_fp to initialise entropy.";
    return false;
  }

  // Initialize the secret.
  if (!AddEntropy(reset)) {
    LOG(ERROR) << "Failed to add entropy.";
    return false;
  }

  // Entropy added, reboot cros_fp to RW.
  if (!EcReboot(EC_IMAGE_RW)) {
    LOG(ERROR) << "Failed to reboot cros_fp after initializing entropy.";
    return false;
  }

  int32_t new_block_id;
  if (!GetRollBackInfoId(&new_block_id)) {
    LOG(ERROR) << "Failed to block ID from FPMCU after entropy reset.";
    return false;
  }

  int32_t block_id_diff = 2;
  if (!reset) {
    block_id_diff = 1;
  }

  if (new_block_id != block_id + block_id_diff) {
    LOG(ERROR) << "Entropy source has not been updated; old block_id: "
               << block_id << ", new block_id: " << new_block_id;
    return false;
  }
  return true;
}

}  // namespace biod
