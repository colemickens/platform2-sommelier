// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <archive.h>
#include <archive_entry.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <utility>

#include <base/files/file_util.h>
#include <base/guid.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>

#include "vm_tools/concierge/disk_image.h"

namespace {

constexpr gid_t kPluginVmGid = 20128;

}  // namespace

namespace vm_tools {
namespace concierge {

DiskImageOperation::DiskImageOperation()
    : uuid_(base::GenerateGUID()), status_(DISK_STATUS_FAILED) {
  CHECK(base::IsValidGUID(uuid_));
}

std::unique_ptr<PluginVmImportOperation> PluginVmImportOperation::Create(
    base::ScopedFD fd, const base::FilePath& disk_path, uint64_t source_size) {
  auto op =
      base::WrapUnique(new PluginVmImportOperation(std::move(fd), source_size));

  if (op->PrepareInput() && op->PrepareOutput(disk_path)) {
    op->set_status(DISK_STATUS_IN_PROGRESS);
  }

  return op;
}

PluginVmImportOperation::PluginVmImportOperation(base::ScopedFD in_fd,
                                                 uint64_t source_size)
    : in_fd_(std::move(in_fd)),
      source_size_(source_size),
      processed_size_(0),
      last_report_level_(0),
      copying_data_(false) {}

bool PluginVmImportOperation::PrepareInput() {
  in_ = ArchiveReader(archive_read_new());
  if (!in_.get()) {
    set_failure_reason("libarchive: failed to create reader");
    return false;
  }

  int ret = archive_read_support_format_zip(in_.get());
  if (ret != ARCHIVE_OK) {
    set_failure_reason("libarchive: failed to initialize zip format");
    return false;
  }

  ret = archive_read_support_filter_all(in_.get());
  if (ret != ARCHIVE_OK) {
    set_failure_reason("libarchive: failed to initialize filter");
    return false;
  }

  ret = archive_read_open_fd(in_.get(), in_fd_.get(), 102400);
  if (ret != ARCHIVE_OK) {
    set_failure_reason("failed to open input archive");
    return false;
  }

  return true;
}

bool PluginVmImportOperation::PrepareOutput(const base::FilePath& disk_path) {
  base::File::Error dir_error;
  if (!base::CreateDirectoryAndGetError(disk_path, &dir_error)) {
    set_failure_reason(std::string("failed to create output directory: ") +
                       base::File::ErrorToString(dir_error));
    return false;
  }

  CHECK(output_dir_.Set(disk_path));

  if (chown(output_dir_.GetPath().value().c_str(), -1, kPluginVmGid) < 0) {
    set_failure_reason("failed to change group of the destination directory");
    return false;
  }

  if (chmod(output_dir_.GetPath().value().c_str(), 0770) < 0) {
    set_failure_reason(
        "failed to change permissions of the destination directory");
    return false;
  }

  out_ = ArchiveWriter(archive_write_disk_new());
  if (!out_) {
    set_failure_reason("libarchive: failed to create writer");
    return false;
  }

  int ret = archive_write_disk_set_options(
      out_.get(), ARCHIVE_EXTRACT_SECURE_SYMLINKS |
                      ARCHIVE_EXTRACT_SECURE_NODOTDOT | ARCHIVE_EXTRACT_OWNER);
  if (ret != ARCHIVE_OK) {
    set_failure_reason("libarchive: failed to initialize filter");
    return false;
  }

  return true;
}

int PluginVmImportOperation::GetProgress() const {
  if (status() == DISK_STATUS_IN_PROGRESS) {
    if (source_size_ == 0)
      return 0;  // We do not know any better.

    return processed_size_ * 100 / source_size_;
  }

  // Any other status indicates completed operation (successfully or not)
  // so return 100%.
  return 100;
}

void PluginVmImportOperation::MarkFailed(const char* msg, struct archive* a) {
  set_status(DISK_STATUS_FAILED);

  if (a) {
    set_failure_reason(
        base::StringPrintf("%s: %s", msg, archive_error_string(a)));
  } else {
    set_failure_reason(msg);
  }

  LOG(ERROR) << "PluginVm import failed: " << failure_reason();

  // Release resources.
  out_.reset();
  if (!output_dir_.Delete()) {
    LOG(WARNING) << "Failed to delete output directory on error";
  }

  in_.reset();
  in_fd_.reset();
}

bool PluginVmImportOperation::CopyImage(uint64_t io_limit) {
  do {
    if (!copying_data_) {
      struct archive_entry* entry;
      int ret = archive_read_next_header(in_.get(), &entry);
      if (ret == ARCHIVE_EOF) {
        // Successfully copied entire archive.
        return true;
      }

      if (ret < ARCHIVE_OK) {
        MarkFailed("failed to read header", in_.get());
        break;
      }

      const char* c_path = archive_entry_pathname(entry);
      if (!c_path || c_path[0] == '\0') {
        MarkFailed("archive entry has empty file name", NULL);
        break;
      }

      base::FilePath path(c_path);
      if (path.empty() || path.IsAbsolute() || path.ReferencesParent()) {
        MarkFailed(
            "archive entry has invalid/absolute/referencing parent file name",
            NULL);
        break;
      }

      auto dest_path = output_dir_.GetPath().Append(path);
      archive_entry_set_pathname(entry, dest_path.value().c_str());

      archive_entry_set_uid(entry, getuid());
      archive_entry_set_gid(entry, kPluginVmGid);

      mode_t mode = archive_entry_filetype(entry);
      switch (mode) {
        case AE_IFREG:
          archive_entry_set_perm(entry, 0660);
          break;
        case AE_IFDIR:
          archive_entry_set_perm(entry, 0770);
          break;
      }

      ret = archive_write_header(out_.get(), entry);
      if (ret != ARCHIVE_OK) {
        MarkFailed("failed to write header", out_.get());
        break;
      }

      copying_data_ = archive_entry_size(entry) > 0;
    }

    if (copying_data_) {
      uint64_t bytes_read = CopyEntry(io_limit);
      io_limit -= std::min(bytes_read, io_limit);
      processed_size_ += bytes_read;
    }

    if (!copying_data_) {
      int ret = archive_write_finish_entry(out_.get());
      if (ret != ARCHIVE_OK) {
        MarkFailed("failed to finish entry", out_.get());
        break;
      }
    }
  } while (io_limit > 0);

  // More copying is to be done (or there was a failure).
  return false;
}

uint64_t PluginVmImportOperation::CopyEntry(uint64_t io_limit) {
  uint64_t bytes_read_begin = archive_filter_bytes(in_.get(), -1);
  uint64_t bytes_read = 0;

  do {
    const void* buff;
    size_t size;
    la_int64_t offset;
    int ret = archive_read_data_block(in_.get(), &buff, &size, &offset);
    if (ret == ARCHIVE_EOF) {
      copying_data_ = false;
      break;
    }

    if (ret != ARCHIVE_OK) {
      MarkFailed("failed to read data block", in_.get());
      break;
    }

    bytes_read = archive_filter_bytes(in_.get(), -1) - bytes_read_begin;

    ret = archive_write_data_block(out_.get(), buff, size, offset);
    if (ret != ARCHIVE_OK) {
      MarkFailed("failed to write data block", out_.get());
      break;
    }
  } while (bytes_read < io_limit);

  return bytes_read;
}

void PluginVmImportOperation::FinalizeCopy() {
  archive_read_close(in_.get());
  // Free the input archive.
  in_.reset();
  // Close the file descriptor.
  in_fd_.reset();

  int ret = archive_write_close(out_.get());
  if (ret != ARCHIVE_OK) {
    MarkFailed("libarchive: failed to close writer", out_.get());
    return;
  }
  out_.reset();
  // Commit to keeping the output directory.
  output_dir_.Take();

  set_status(DISK_STATUS_CREATED);
}

bool PluginVmImportOperation::Run(uint64_t io_limit) {
  if (CopyImage(io_limit)) {
    FinalizeCopy();
    return true;
  }

  // Signal that clients can be notified if we advanced more than 5% since last
  // notification.
  int current_progress = GetProgress();
  if (current_progress - last_report_level_ >= 5) {
    last_report_level_ = current_progress;
    return true;
  }

  return false;
}

}  // namespace concierge
}  // namespace vm_tools
