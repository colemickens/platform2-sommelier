// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/cros_fp_biometrics_manager.h"

#include <algorithm>
#include <utility>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/base64.h>
#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_piece.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <crypto/random.h>
#include <metrics/metrics_library.h>

#include "biod/biod_metrics.h"
#include "biod/uinput_device.h"

using MessageLoopForIO = base::MessageLoopForIO;

using VendorTemplate = std::vector<uint8_t>;

namespace {

std::string FourCC(const uint32_t a) {
  return base::StringPrintf(
      "%c%c%c%c", static_cast<char>(a), static_cast<char>(a >> 8),
      static_cast<char>(a >> 16), static_cast<char>(a >> 24));
}

const int kLastTemplate = -1;

// Upper bound of the host command packet transfer size.
const int kMaxPacketSize = 544;

};  // namespace

namespace biod {

constexpr char CrosFpBiometricsManager::kCrosFpPath[];

class CrosFpBiometricsManager::CrosFpDevice : public MessageLoopForIO::Watcher {
 public:
  using MkbpCallback = base::Callback<void(const uint32_t event)>;

  explicit CrosFpDevice(const MkbpCallback& mkbp_event)
      : mkbp_event_(mkbp_event),
        fd_watcher_(std::make_unique<MessageLoopForIO::FileDescriptorWatcher>(
            FROM_HERE)) {}
  ~CrosFpDevice();

  static std::unique_ptr<CrosFpDevice> Open(const MkbpCallback& callback);

  bool FpMode(uint32_t mode);
  bool GetFpMode(uint32_t* mode);
  bool GetFpStats(int* capture_ms, int* matcher_ms, int* overall_ms);
  bool GetDirtyMap(std::bitset<32>* bitmap);
  bool GetTemplate(int index, VendorTemplate* out);
  bool UploadTemplate(const VendorTemplate& tmpl);
  bool SetContext(std::string user_id);
  bool ResetContext();
  // Initialise the entropy in the SBP. If |reset| is true, the old entropy will
  // be deleted. If |reset| is false, we will only add entropy, and only if no
  // entropy had been added before.
  bool InitEntropy(bool reset = false);

  int MaxTemplateCount() { return info_.template_max; }
  int TemplateVersion() { return info_.template_version; }

 protected:
  // MessageLoopForIO::Watcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override {}

 private:
  bool Init();

  bool EcDevInit();
  bool EcProtoInfo(ssize_t* max_read, ssize_t* max_write);
  bool WaitOnEcBoot(ec_current_image expected_image);
  bool EcReboot(ec_current_image to_image);
  // Run the EC command to generate new entropy in the underlying MCU.
  // |reset| specifies whether we want to merely add entropy (false), or perform
  // a reset, which erases old entropy(true).
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
  ssize_t max_read_size_;
  ssize_t max_write_size_;
  struct ec_response_fp_info info_;

  MkbpCallback mkbp_event_;
  UinputDevice input_device_;

  std::unique_ptr<MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  DISALLOW_COPY_AND_ASSIGN(CrosFpDevice);
};

CrosFpBiometricsManager::CrosFpDevice::~CrosFpDevice() {
  // Current session is gone, clean-up temporary state in the FP MCU.
  if (cros_fd_.is_valid())
    ResetContext();
}

std::unique_ptr<CrosFpBiometricsManager::CrosFpDevice>
CrosFpBiometricsManager::CrosFpDevice::Open(const MkbpCallback& callback) {
  std::unique_ptr<CrosFpDevice> dev = std::make_unique<CrosFpDevice>(callback);
  if (!dev->Init())
    return nullptr;
  return dev;
}

bool CrosFpBiometricsManager::CrosFpDevice::EcProtoInfo(ssize_t* max_read,
                                                        ssize_t* max_write) {
  /* read max request / response size from the MCU for protocol v3+ */
  EcCommand<EmptyParam, struct ec_response_get_protocol_info> cmd(
      EC_CMD_GET_PROTOCOL_INFO);
  if (!cmd.Run(cros_fd_.get()))
    return false;

  *max_read =
      cmd.Resp()->max_response_packet_size - sizeof(struct ec_host_response);
  // TODO(vpalatin): workaround for b/78544921, can be removed if MCU is fixed.
  *max_write =
      cmd.Resp()->max_request_packet_size - sizeof(struct ec_host_request) - 4;
  return true;
}

bool CrosFpBiometricsManager::CrosFpDevice::EcDevInit() {
  char version[80];
  int ret = read(cros_fd_.get(), version, sizeof(version) - 1);
  if (ret <= 0) {
    int err = ret ? errno : 0;
    LOG(ERROR) << "Failed to read cros_fp device: " << err;
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
    LOG(ERROR) << "Fail to get cros_fp protocol info " << ret;
    return false;
  }

  unsigned long mask = 1 << EC_MKBP_EVENT_FINGERPRINT;  // NOLINT(runtime/int)
  if (ioctl(cros_fd_.get(), CROS_EC_DEV_IOCEVENTMASK_V2, mask) < 0) {
    LOG(ERROR) << "Fail to request fingerprint events";
    return false;
  }

  return true;
}

void CrosFpBiometricsManager::CrosFpDevice::OnFileCanReadWithoutBlocking(
    int fd) {
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

bool CrosFpBiometricsManager::CrosFpDevice::FpMode(uint32_t mode) {
  EcCommand<struct ec_params_fp_mode, struct ec_response_fp_mode> cmd(
      EC_CMD_FP_MODE, 0, {.mode = mode});
  return cmd.Run(cros_fd_.get());
}

bool CrosFpBiometricsManager::CrosFpDevice::GetFpMode(uint32_t* mode) {
  EcCommand<struct ec_params_fp_mode, struct ec_response_fp_mode> cmd(
      EC_CMD_FP_MODE, 0, {.mode = static_cast<uint32_t>(FP_MODE_DONT_CHANGE)});
  if (!cmd.Run(cros_fd_.get())) {
    LOG(ERROR) << "Failed to get FP mode from MCU.";
    return false;
  }

  *mode = cmd.Resp()->mode;
  return true;
}

bool CrosFpBiometricsManager::CrosFpDevice::FpFrame(
    int index, std::vector<uint8_t>* frame) {
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

bool CrosFpBiometricsManager::CrosFpDevice::UpdateFpInfo() {
  EcCommand<EmptyParam, struct ec_response_fp_info> cmd(EC_CMD_FP_INFO, 1);
  if (!cmd.Run(cros_fd_.get())) {
    LOG(ERROR) << "Failed to get FP information";
    return false;
  }
  info_ = *cmd.Resp();
  return true;
}

bool CrosFpBiometricsManager::CrosFpDevice::GetFpStats(int* capture_ms,
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

bool CrosFpBiometricsManager::CrosFpDevice::WaitOnEcBoot(
    ec_current_image expected_image) {
  int tries = 50;
  ec_current_image image = EC_IMAGE_UNKNOWN;

  while (tries) {
    tries--;
    // Check the EC has the right image.
    EcCommand<EmptyParam, struct ec_response_get_version> cmd(
        EC_CMD_GET_VERSION);
    if (!cmd.Run(cros_fd_.get())) {
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

bool CrosFpBiometricsManager::CrosFpDevice::EcReboot(
    ec_current_image to_image) {
  DCHECK(to_image == EC_IMAGE_RO || to_image == EC_IMAGE_RW);

  EcCommand<EmptyParam, EmptyParam> cmd_reboot(EC_CMD_REBOOT);
  // Don't expect a return code, cros_fp has rebooted.
  cmd_reboot.Run(cros_fd_.get());

  if (!WaitOnEcBoot(EC_IMAGE_RO)) {
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

  if (!WaitOnEcBoot(to_image)) {
    LOG(ERROR) << "EC did not load the right image.";
    return false;
  }

  return true;
}

bool CrosFpBiometricsManager::CrosFpDevice::AddEntropy(bool reset) {
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

bool CrosFpBiometricsManager::CrosFpDevice::GetRollBackInfoId(
    int32_t* block_id) {
  EcCommand<EmptyParam, struct ec_response_rollback_info> cmd_rb_info(
      EC_CMD_ROLLBACK_INFO);
  if (!cmd_rb_info.Run(cros_fd_.get())) {
    return false;
  }

  *block_id = cmd_rb_info.Resp()->id;
  return true;
}

bool CrosFpBiometricsManager::CrosFpDevice::InitEntropy(bool reset) {
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

bool CrosFpBiometricsManager::CrosFpDevice::Init() {
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

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          cros_fd_.get(), true, MessageLoopForIO::WATCH_READ, fd_watcher_.get(),
          this)) {
    LOG(ERROR) << "Unable to watch MKBP events";
    return false;
  }

  if (!input_device_.Init()) {
    LOG(ERROR) << "Failed to create Uinput device";
    return false;
  }

  return true;
}

bool CrosFpBiometricsManager::CrosFpDevice::GetDirtyMap(
    std::bitset<32>* bitmap) {
  // Retrieve the up-to-date dirty bitmap from the MCU.
  if (!UpdateFpInfo())
    return false;

  *bitmap = std::bitset<32>(info_.template_dirty);
  return true;
}

bool CrosFpBiometricsManager::CrosFpDevice::GetTemplate(int index,
                                                        VendorTemplate* out) {
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

bool CrosFpBiometricsManager::CrosFpDevice::UploadTemplate(
    const VendorTemplate& tmpl) {
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

bool CrosFpBiometricsManager::CrosFpDevice::SetContext(std::string user_hex) {
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

bool CrosFpBiometricsManager::CrosFpDevice::ResetContext() {
  return SetContext(std::string());
}

bool CrosFpBiometricsManager::CrosFpDevice::UpdateEntropy(bool reset) {
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

const std::string& CrosFpBiometricsManager::Record::GetId() const {
  CHECK(biometrics_manager_);
  CHECK(index_ < biometrics_manager_->records_.size());
  local_record_id_ = biometrics_manager_->records_[index_].record_id;
  return local_record_id_;
}

const std::string& CrosFpBiometricsManager::Record::GetUserId() const {
  CHECK(biometrics_manager_);
  CHECK(index_ <= biometrics_manager_->records_.size());
  local_user_id_ = biometrics_manager_->records_[index_].user_id;
  return local_user_id_;
}

const std::string& CrosFpBiometricsManager::Record::GetLabel() const {
  CHECK(biometrics_manager_);
  CHECK(index_ < biometrics_manager_->records_.size());
  local_label_ = biometrics_manager_->records_[index_].label;
  return local_label_;
}

bool CrosFpBiometricsManager::Record::SetLabel(std::string label) {
  CHECK(biometrics_manager_);
  CHECK(index_ < biometrics_manager_->records_.size());
  std::string old_label = biometrics_manager_->records_[index_].label;

  VendorTemplate tmpl;
  // TODO(vpalatin): would be faster to read it from disk
  if (!biometrics_manager_->cros_dev_->GetTemplate(index_, &tmpl))
    return false;
  biometrics_manager_->records_[index_].label = std::move(label);

  if (!biometrics_manager_->WriteRecord(*this, tmpl.data(), tmpl.size())) {
    biometrics_manager_->records_[index_].label = std::move(old_label);
    return false;
  }
  return true;
}

bool CrosFpBiometricsManager::Record::Remove() {
  if (!biometrics_manager_)
    return false;
  if (index_ >= biometrics_manager_->records_.size())
    return false;

  std::vector<InternalRecord>::iterator record =
      biometrics_manager_->records_.begin() + index_;
  std::string user_id = record->user_id;

  // TODO(mqg): only delete record if user_id is primary user.
  if (!biometrics_manager_->biod_storage_.DeleteRecord(user_id,
                                                       record->record_id))
    return false;

  // We cannot remove only one record if we want to stay in sync with the MCU,
  // Clear and reload everything.
  biometrics_manager_->records_.clear();
  biometrics_manager_->cros_dev_->SetContext(user_id);
  return biometrics_manager_->biod_storage_.ReadRecordsForSingleUser(user_id);
}

std::unique_ptr<BiometricsManager> CrosFpBiometricsManager::Create() {
  std::unique_ptr<CrosFpBiometricsManager> biometrics_manager(
      new CrosFpBiometricsManager);
  if (!biometrics_manager->Init())
    return nullptr;

  return biometrics_manager;
}

BiometricType CrosFpBiometricsManager::GetType() {
  return BIOMETRIC_TYPE_FINGERPRINT;
}

BiometricsManager::EnrollSession CrosFpBiometricsManager::StartEnrollSession(
    std::string user_id, std::string label) {
  LOG(INFO) << __func__;
  // Another session is on-going, fail early ...
  if (!next_session_action_.is_null())
    return BiometricsManager::EnrollSession();

  if (records_.size() >= cros_dev_->MaxTemplateCount()) {
    LOG(ERROR) << "No space for an additional template.";
    return BiometricsManager::EnrollSession();
  }

  if (!RequestEnrollImage(InternalRecord{biod_storage_.GenerateNewRecordId(),
                                         std::move(user_id), std::move(label)}))
    return BiometricsManager::EnrollSession();

  return BiometricsManager::EnrollSession(session_weak_factory_.GetWeakPtr());
}

BiometricsManager::AuthSession CrosFpBiometricsManager::StartAuthSession() {
  LOG(INFO) << __func__;
  // Another session is on-going, fail early ...
  if (!next_session_action_.is_null())
    return BiometricsManager::AuthSession();

  if (!RequestMatch())
    return BiometricsManager::AuthSession();

  return BiometricsManager::AuthSession(session_weak_factory_.GetWeakPtr());
}

std::vector<std::unique_ptr<BiometricsManager::Record>>
CrosFpBiometricsManager::GetRecords() {
  std::vector<std::unique_ptr<BiometricsManager::Record>> records;
  for (int i = 0; i < records_.size(); i++)
    records.emplace_back(std::unique_ptr<BiometricsManager::Record>(
        new Record(weak_factory_.GetWeakPtr(), i)));
  return records;
}

bool CrosFpBiometricsManager::DestroyAllRecords() {
  // Enumerate through records_ and delete each record.
  bool delete_all_records = true;
  for (auto& record : records_) {
    delete_all_records &=
        biod_storage_.DeleteRecord(record.user_id, record.record_id);
  }
  RemoveRecordsFromMemory();
  return delete_all_records;
}

void CrosFpBiometricsManager::RemoveRecordsFromMemory() {
  records_.clear();
  cros_dev_->ResetContext();
}

bool CrosFpBiometricsManager::ReadRecords(
    const std::unordered_set<std::string>& user_ids) {
  // TODO(mqg): delete this function and adjust parent class accordingly.
  LOG(WARNING) << __func__ << " should not be used.";
  return false;
}

bool CrosFpBiometricsManager::ReadRecordsForSingleUser(
    const std::string& user_id) {
  cros_dev_->SetContext(user_id);
  return biod_storage_.ReadRecordsForSingleUser(user_id);
}

void CrosFpBiometricsManager::SetEnrollScanDoneHandler(
    const BiometricsManager::EnrollScanDoneCallback& on_enroll_scan_done) {
  on_enroll_scan_done_ = on_enroll_scan_done;
}

void CrosFpBiometricsManager::SetAuthScanDoneHandler(
    const BiometricsManager::AuthScanDoneCallback& on_auth_scan_done) {
  on_auth_scan_done_ = on_auth_scan_done;
}

void CrosFpBiometricsManager::SetSessionFailedHandler(
    const BiometricsManager::SessionFailedCallback& on_session_failed) {
  on_session_failed_ = on_session_failed;
}

bool CrosFpBiometricsManager::SendStatsOnLogin() {
  bool rc = true;
  rc = biod_metrics_->SendEnrolledFingerCount(records_.size()) && rc;
  // Even though it looks a bit redundant with the finger count, it's easier to
  // discover and interpret.
  rc = biod_metrics_->SendFpUnlockEnabled(!records_.empty()) && rc;
  return rc;
}

void CrosFpBiometricsManager::SetDiskAccesses(bool allow) {
  biod_storage_.set_allow_access(allow);
}

bool CrosFpBiometricsManager::ResetSensor() {
  if (!cros_dev_->FpMode(FP_MODE_RESET_SENSOR)) {
    LOG(ERROR) << "Failed to send reset_sensor command to FPMCU.";
    return false;
  }

  int retries = 50;
  bool reset_complete = false;
  while (retries--) {
    uint32_t cur_mode;
    if (!cros_dev_->GetFpMode(&cur_mode)) {
      LOG(ERROR) << "Failed to query sensor state during reset.";
      return false;
    }

    if (!(cur_mode & FP_MODE_RESET_SENSOR)) {
      reset_complete = true;
      break;
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  }

  if (!reset_complete) {
    LOG(ERROR) << "Reset on FPMCU failed to complete.";
    return false;
  }

  if (!Init()) {
    LOG(ERROR) << "Failed to reinitialize CrosFpBiometricsManager.";
    return false;
  }

  return true;
}

bool CrosFpBiometricsManager::ResetEntropy(bool factory_init) {
  bool success = cros_dev_->InitEntropy(!factory_init);
  if (!success) {
    LOG(INFO) << "Entropy source reset failed.";
    return false;
  }
  LOG(INFO) << "Entropy source has been successfully reset.";
  return true;
}

void CrosFpBiometricsManager::EndEnrollSession() {
  LOG(INFO) << __func__;
  KillMcuSession();
}

void CrosFpBiometricsManager::EndAuthSession() {
  LOG(INFO) << __func__;
  KillMcuSession();
}

void CrosFpBiometricsManager::KillMcuSession() {
  // TODO(vpalatin): test cros_dev_->FpMode(FP_MODE_DEEPSLEEP);
  cros_dev_->FpMode(0);
  session_weak_factory_.InvalidateWeakPtrs();
  OnTaskComplete();
}

CrosFpBiometricsManager::CrosFpBiometricsManager()
    : session_weak_factory_(this),
      weak_factory_(this),
      biod_metrics_(std::make_unique<BiodMetrics>()),
      biod_storage_(kCrosFpBiometricsManagerName,
                    base::Bind(&CrosFpBiometricsManager::LoadRecord,
                               base::Unretained(this))) {}

CrosFpBiometricsManager::~CrosFpBiometricsManager() {}

bool CrosFpBiometricsManager::Init() {
  cros_dev_ = CrosFpDevice::Open(base::Bind(
      &CrosFpBiometricsManager::OnMkbpEvent, base::Unretained(this)));
  return !!cros_dev_;
}

void CrosFpBiometricsManager::OnEnrollScanDone(
    ScanResult result, const BiometricsManager::EnrollStatus& enroll_status) {
  if (!on_enroll_scan_done_.is_null())
    on_enroll_scan_done_.Run(result, enroll_status);
}

void CrosFpBiometricsManager::OnAuthScanDone(
    ScanResult result, const BiometricsManager::AttemptMatches& matches) {
  if (!on_auth_scan_done_.is_null())
    on_auth_scan_done_.Run(result, matches);
}

void CrosFpBiometricsManager::OnSessionFailed() {
  if (!on_session_failed_.is_null())
    on_session_failed_.Run();
}

void CrosFpBiometricsManager::OnMkbpEvent(uint32_t event) {
  if (!next_session_action_.is_null())
    next_session_action_.Run(event);
}

bool CrosFpBiometricsManager::RequestEnrollImage(InternalRecord record) {
  next_session_action_ =
      base::Bind(&CrosFpBiometricsManager::DoEnrollImageEvent,
                 base::Unretained(this), std::move(record));
  if (!cros_dev_->FpMode(FP_MODE_ENROLL_SESSION | FP_MODE_ENROLL_IMAGE)) {
    next_session_action_ = SessionAction();
    LOG(ERROR) << "Failed to start enrolling mode";
    return false;
  }
  return true;
}

bool CrosFpBiometricsManager::RequestEnrollFingerUp(InternalRecord record) {
  next_session_action_ =
      base::Bind(&CrosFpBiometricsManager::DoEnrollFingerUpEvent,
                 base::Unretained(this), std::move(record));
  if (!cros_dev_->FpMode(FP_MODE_ENROLL_SESSION | FP_MODE_FINGER_UP)) {
    next_session_action_ = SessionAction();
    LOG(ERROR) << "Failed to wait for finger up";
    return false;
  }
  return true;
}

bool CrosFpBiometricsManager::RequestMatch(int attempt) {
  next_session_action_ = base::Bind(&CrosFpBiometricsManager::DoMatchEvent,
                                    base::Unretained(this), attempt);
  if (!cros_dev_->FpMode(FP_MODE_MATCH)) {
    next_session_action_ = SessionAction();
    LOG(ERROR) << "Failed to start matching mode";
    return false;
  }
  return true;
}

bool CrosFpBiometricsManager::RequestMatchFingerUp() {
  next_session_action_ = base::Bind(
      &CrosFpBiometricsManager::DoMatchFingerUpEvent, base::Unretained(this));
  if (!cros_dev_->FpMode(FP_MODE_FINGER_UP)) {
    next_session_action_ = SessionAction();
    LOG(ERROR) << "Failed to request finger up event";
    return false;
  }
  return true;
}

void CrosFpBiometricsManager::DoEnrollImageEvent(InternalRecord record,
                                                 uint32_t event) {
  if (!(event & EC_MKBP_FP_ENROLL)) {
    LOG(WARNING) << "Unexpected MKBP event: 0x" << std::hex << event;
    // Continue waiting for the proper event, do not abort session.
    return;
  }

  int image_result = EC_MKBP_FP_ERRCODE(event);
  LOG(INFO) << __func__ << " result: " << image_result;
  ScanResult scan_result;
  switch (image_result) {
    case EC_MKBP_FP_ERR_ENROLL_OK:
      scan_result = ScanResult::SCAN_RESULT_SUCCESS;
      break;
    case EC_MKBP_FP_ERR_ENROLL_IMMOBILE:
      scan_result = ScanResult::SCAN_RESULT_IMMOBILE;
      break;
    case EC_MKBP_FP_ERR_ENROLL_LOW_COVERAGE:
      scan_result = ScanResult::SCAN_RESULT_PARTIAL;
      break;
    case EC_MKBP_FP_ERR_ENROLL_LOW_QUALITY:
      scan_result = ScanResult::SCAN_RESULT_INSUFFICIENT;
      break;
    case EC_MKBP_FP_ERR_ENROLL_INTERNAL:
    default:
      LOG(ERROR) << "Unexpected result from capture: " << std::hex << event;
      OnSessionFailed();
      return;
  }

  int percent = EC_MKBP_FP_ENROLL_PROGRESS(event);

  if (percent < 100) {
    BiometricsManager::EnrollStatus enroll_status = {false, percent};

    OnEnrollScanDone(scan_result, enroll_status);

    // The user needs to remove the finger before the next enrollment image.
    if (!RequestEnrollFingerUp(std::move(record)))
      OnSessionFailed();

    return;
  }

  // we are done with captures, save the template.
  OnTaskComplete();

  VendorTemplate tmpl;
  if (!cros_dev_->GetTemplate(kLastTemplate, &tmpl)) {
    LOG(ERROR) << "Failed to retrieve enrolled finger";
    OnSessionFailed();
    return;
  }

  records_.emplace_back(record);
  Record current_record(weak_factory_.GetWeakPtr(), records_.size() - 1);
  if (!WriteRecord(current_record, tmpl.data(), tmpl.size())) {
    records_.pop_back();
    OnSessionFailed();
    return;
  }

  BiometricsManager::EnrollStatus enroll_status = {true, 100};
  OnEnrollScanDone(ScanResult::SCAN_RESULT_SUCCESS, enroll_status);
}

void CrosFpBiometricsManager::DoEnrollFingerUpEvent(InternalRecord record,
                                                    uint32_t event) {
  if (!(event & EC_MKBP_FP_FINGER_UP)) {
    LOG(WARNING) << "Unexpected MKBP event: 0x" << std::hex << event;
    // Continue waiting for the proper event, do not abort session.
    return;
  }

  if (!RequestEnrollImage(std::move(record)))
    OnSessionFailed();
}

void CrosFpBiometricsManager::DoMatchFingerUpEvent(uint32_t event) {
  if (!(event & EC_MKBP_FP_FINGER_UP)) {
    LOG(WARNING) << "Unexpected MKBP event: 0x" << std::hex << event;
    // Continue waiting for the proper event, do not abort session.
    return;
  }
  // The user has lifted their finger, try to match the next touch.
  if (!RequestMatch())
    OnSessionFailed();
}

void CrosFpBiometricsManager::DoMatchEvent(int attempt, uint32_t event) {
  if (!(event & EC_MKBP_FP_MATCH)) {
    LOG(WARNING) << "Unexpected MKBP event: 0x" << std::hex << event;
    // Continue waiting for the proper event, do not abort session.
    return;
  }

  ScanResult result;
  int match_result = EC_MKBP_FP_ERRCODE(event);

  // If the finger is positioned slightly off the sensor, retry a few times
  // before failing. Typically the user has put their finger down and is now
  // moving their finger to the correct position on the sensor. Instead of
  // immediately failing, retry until we get a good image.
  // Retry 20 times, which takes about 5 to 15s, before giving up and sending
  // an error back to the user. Assume ~1s for user noticing that no matching
  // happened, some time to move the finger on the sensor to allow a full
  // capture and another 1s for the second matching attempt. 5s gives a bit of
  // margin to avoid interrupting the user while they're moving the finger on
  // the sensor.
  const int kMaxPartialAttempts = 20;

  if (match_result == EC_MKBP_FP_ERR_MATCH_NO_LOW_COVERAGE &&
      attempt < kMaxPartialAttempts) {
    /* retry a match */
    if (!RequestMatch(attempt + 1))
      OnSessionFailed();
    return;
  }

  // Don't try to match again until the user has lifted their finger from the
  // sensor. Request the FingerUp event as soon as the HW signaled a match so it
  // doesn't attempt a new match while the host is processing the first
  // match event.
  if (!RequestMatchFingerUp()) {
    OnSessionFailed();
    return;
  }

  std::vector<int> dirty_list;
  if (match_result == EC_MKBP_FP_ERR_MATCH_YES_UPDATED) {
    std::bitset<32> dirty_bitmap(0);
    // Retrieve which templates have been updated.
    if (!cros_dev_->GetDirtyMap(&dirty_bitmap))
      LOG(ERROR) << "Failed to get updated templates map";
    // Create a list of modified template indexes from the bitmap.
    dirty_list.reserve(dirty_bitmap.count());
    for (int i = 0; dirty_bitmap.any() && i < dirty_bitmap.size(); i++)
      if (dirty_bitmap[i]) {
        dirty_list.emplace_back(i);
        dirty_bitmap.reset(i);
      }
  }

  BiometricsManager::AttemptMatches matches;
  std::vector<std::string> records;

  uint32_t match_idx = EC_MKBP_FP_MATCH_IDX(event);
  LOG(INFO) << __func__ << " result: " << match_result
            << " (finger: " << match_idx << ")";
  switch (match_result) {
    case EC_MKBP_FP_ERR_MATCH_NO_INTERNAL:
      LOG(ERROR) << "Internal error when matching templates: " << std::hex
                 << event;
    // Fall-through.
    case EC_MKBP_FP_ERR_MATCH_NO:
      // This is the API: empty matches but still SCAN_RESULT_SUCCESS.
      result = ScanResult::SCAN_RESULT_SUCCESS;
      break;
    case EC_MKBP_FP_ERR_MATCH_YES:
    case EC_MKBP_FP_ERR_MATCH_YES_UPDATED:
    case EC_MKBP_FP_ERR_MATCH_YES_UPDATE_FAILED:
      result = ScanResult::SCAN_RESULT_SUCCESS;

      if (match_idx < records_.size()) {
        records.push_back(records_[match_idx].record_id);
        matches.emplace(records_[match_idx].user_id, std::move(records));
      } else {
        LOG(ERROR) << "Invalid finger index " << match_idx;
      }
      break;
    case EC_MKBP_FP_ERR_MATCH_NO_LOW_QUALITY:
      result = ScanResult::SCAN_RESULT_INSUFFICIENT;
      break;
    case EC_MKBP_FP_ERR_MATCH_NO_LOW_COVERAGE:
      result = ScanResult::SCAN_RESULT_PARTIAL;
      break;
    default:
      LOG(ERROR) << "Unexpected result from matching templates: " << std::hex
                 << event;
      OnSessionFailed();
      return;
  }

  // Send back the result directly (as we are running on the main thread).
  OnAuthScanDone(result, std::move(matches));

  int capture_ms, matcher_ms, overall_ms;
  if (cros_dev_->GetFpStats(&capture_ms, &matcher_ms, &overall_ms)) {
    // SCAN_RESULT_SUCCESS and EC_MKBP_FP_ERR_MATCH_NO means "no match".
    bool matched = (result == ScanResult::SCAN_RESULT_SUCCESS &&
                    match_result != EC_MKBP_FP_ERR_MATCH_NO);
    biod_metrics_->SendFpLatencyStats(matched, capture_ms, matcher_ms,
                                      overall_ms);
  }

  // Record updated templates
  // TODO(vpalatin): this is slow, move to end of session ?
  for (int i : dirty_list) {
    VendorTemplate templ;
    bool rc = cros_dev_->GetTemplate(i, &templ);
    LOG(INFO) << "Retrieve updated template " << i << " -> " << rc;
    if (!rc)
      continue;

    Record current_record(weak_factory_.GetWeakPtr(), i);
    if (!WriteRecord(current_record, templ.data(), templ.size())) {
      LOG(ERROR) << "Cannot update record " << records_[i].record_id
                 << " in storage during AuthSession because writing failed.";
    }
  }
}

void CrosFpBiometricsManager::OnTaskComplete() {
  next_session_action_ = SessionAction();
}

bool CrosFpBiometricsManager::LoadRecord(const std::string& user_id,
                                         const std::string& label,
                                         const std::string& record_id,
                                         const base::Value& data) {
  std::string tmpl_data_base64;
  if (!data.GetAsString(&tmpl_data_base64)) {
    LOG(ERROR) << "Cannot load data string from record " << record_id << ".";
    return false;
  }

  base::StringPiece tmpl_data_base64_sp(tmpl_data_base64);
  std::string tmpl_data_str;
  base::Base64Decode(tmpl_data_base64_sp, &tmpl_data_str);

  if (records_.size() >= cros_dev_->MaxTemplateCount()) {
    LOG(ERROR) << "No space to upload template from " << record_id << ".";
    return false;
  }

  LOG(INFO) << "Upload record " << record_id;
  VendorTemplate tmpl(tmpl_data_str.begin(), tmpl_data_str.end());
  auto* metadata =
      reinterpret_cast<const ec_fp_template_encryption_metadata*>(tmpl.data());
  if (metadata->struct_version != cros_dev_->TemplateVersion()) {
    LOG(ERROR) << "Version mismatch between template ("
               << metadata->struct_version << ") and hardware ("
               << cros_dev_->TemplateVersion() << ")";
    biod_storage_.DeleteRecord(user_id, record_id);
    return false;
  }
  if (!cros_dev_->UploadTemplate(tmpl)) {
    LOG(ERROR) << "Cannot send template to the MCU from " << record_id << ".";
    return false;
  }

  InternalRecord internal_record = {record_id, user_id, label};
  records_.emplace_back(std::move(internal_record));
  return true;
}

bool CrosFpBiometricsManager::WriteRecord(
    const BiometricsManager::Record& record,
    uint8_t* tmpl_data,
    size_t tmpl_size) {
  base::StringPiece tmpl_sp(reinterpret_cast<char*>(tmpl_data), tmpl_size);
  std::string tmpl_base64;
  base::Base64Encode(tmpl_sp, &tmpl_base64);

  return biod_storage_.WriteRecord(record,
                                   std::make_unique<base::Value>(tmpl_base64));
}
}  // namespace biod
