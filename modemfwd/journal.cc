// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/journal.h"

#include <utility>
#include <vector>

#include <base/files/file.h>
#include <base/strings/string_split.h>
#include <brillo/proto_file_io.h>

#include "modemfwd/modem_helper.h"
#include "modemfwd/proto_bindings/journal_entry.pb.h"

namespace modemfwd {

namespace {

// Returns true if the operation was restarted successfully or false if it
// failed.
bool RestartOperation(const JournalEntry& entry,
                      FirmwareDirectory* firmware_dir,
                      ModemHelperDirectory* helper_dir) {
  ModemHelper* helper = helper_dir->GetHelperForDeviceId(entry.device_id());
  if (!helper) {
    LOG(ERROR) << "Journal contained unfinished operation for device with ID \""
               << entry.device_id()
               << "\" but no helper was found to restart it";
    return false;
  }

  FirmwareFileInfo firmware_file;
  if (!entry.carrier_id().empty()) {
    std::string carrier_id(entry.carrier_id());
    if (!firmware_dir->FindCarrierFirmware(entry.device_id(), &carrier_id,
                                           &firmware_file)) {
      LOG(ERROR) << "Unfinished carrier firmware flash for device with ID \""
                 << entry.device_id() << "\" but no firmware was found";
      return false;
    }
    DLOG(INFO) << "Journal reflashing carrier firmware "
               << firmware_file.firmware_path.value();
    return helper->FlashCarrierFirmware(firmware_file.firmware_path);
  }

  if (!firmware_dir->FindMainFirmware(entry.device_id(), &firmware_file)) {
    LOG(ERROR) << "Unfinished main firmware flash for device with ID \""
               << entry.device_id() << "\" but no firmware was found";
    return false;
  }
  DLOG(INFO) << "Journal reflashing main firmware "
             << firmware_file.firmware_path.value();
  return helper->FlashMainFirmware(firmware_file.firmware_path);
}

class JournalImpl : public Journal {
 public:
  explicit JournalImpl(base::File journal_file)
      : journal_file_(std::move(journal_file)) {
    // Clearing the journal prevents it from growing without bound but also
    // ensures that if we crash after this point, we won't try to restart
    // any operations an extra time.
    ClearJournalFile();
  }

  void MarkStartOfFlashingMainFirmware(const std::string& device_id) override {
    JournalEntry entry;
    entry.set_device_id(device_id);
    WriteJournalEntry(entry);
  }

  void MarkEndOfFlashingMainFirmware(const std::string& device_id) override {
    JournalEntry entry;
    if (!ReadJournalEntry(&entry)) {
      LOG(ERROR) << __func__ << ": no journal entry to commit";
      return;
    }
    if (entry.device_id() != device_id || !entry.carrier_id().empty()) {
      LOG(ERROR) << __func__ << ": found journal entry, but it didn't match";
      return;
    }
    ClearJournalFile();
  }

  void MarkStartOfFlashingCarrierFirmware(
      const std::string& device_id, const std::string& carrier_id) override {
    JournalEntry entry;
    entry.set_device_id(device_id);
    entry.set_carrier_id(carrier_id);
    WriteJournalEntry(entry);
  }

  void MarkEndOfFlashingCarrierFirmware(
      const std::string& device_id, const std::string& carrier_id) override {
    JournalEntry entry;
    if (!ReadJournalEntry(&entry)) {
      LOG(ERROR) << __func__ << ": no journal entry to commit";
      return;
    }
    if (entry.device_id() != device_id || entry.carrier_id() != carrier_id) {
      LOG(ERROR) << __func__ << ": found journal entry, but it didn't match";
      return;
    }
    ClearJournalFile();
  }

 private:
  bool ReadJournalEntry(JournalEntry* entry) {
    if (journal_file_.GetLength() == 0) {
      DLOG(INFO) << "Tried to read from empty journal";
      return false;
    }
    journal_file_.Seek(base::File::FROM_BEGIN, 0);
    return brillo::ReadTextProtobuf(journal_file_.GetPlatformFile(), entry);
  }

  bool WriteJournalEntry(const JournalEntry& entry) {
    if (journal_file_.GetLength() > 0) {
      DLOG(INFO) << "Tried to write to journal with uncommitted entry";
      return false;
    }
    journal_file_.Seek(base::File::FROM_BEGIN, 0);
    return brillo::WriteTextProtobuf(journal_file_.GetPlatformFile(), entry);
  }

  void ClearJournalFile() {
    journal_file_.SetLength(0);
    journal_file_.Seek(base::File::FROM_BEGIN, 0);
    journal_file_.Flush();
  }

  base::File journal_file_;

  DISALLOW_COPY_AND_ASSIGN(JournalImpl);
};

}  // namespace

std::unique_ptr<Journal> OpenJournal(const base::FilePath& journal_path,
                                     FirmwareDirectory* firmware_dir,
                                     ModemHelperDirectory* helper_dir) {
  base::File journal_file(journal_path, base::File::FLAG_OPEN_ALWAYS |
                                            base::File::FLAG_READ |
                                            base::File::FLAG_WRITE);
  if (!journal_file.IsValid()) {
    LOG(ERROR) << "Could not open journal file";
    return nullptr;
  }

  // Restart operations if necessary.
  if (journal_file.GetLength() > 0) {
    JournalEntry last_entry;
    if (brillo::ReadTextProtobuf(journal_file.GetPlatformFile(), &last_entry) &&
        !RestartOperation(last_entry, firmware_dir, helper_dir)) {
      LOG(ERROR) << "Failed to restart uncommitted operation";
    }
  }

  return std::make_unique<JournalImpl>(std::move(journal_file));
}

}  // namespace modemfwd
