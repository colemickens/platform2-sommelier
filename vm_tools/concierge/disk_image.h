// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_DISK_IMAGE_H_
#define VM_TOOLS_CONCIERGE_DISK_IMAGE_H_

#include <archive.h>

#include <memory>
#include <string>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <dbus/exported_object.h>

#include <vm_concierge/proto_bindings/service.pb.h>

namespace vm_tools {
namespace concierge {

class DiskImageOperation {
 public:
  virtual ~DiskImageOperation() = default;

  // Execute next chunk of the disk operation, handling up to |io_limit| bytes.
  // Returns true if enough progress has been made so that caller can notify
  // clients of the progress being made.
  virtual bool Run(uint64_t io_limit) = 0;

  // Report operation progress, in 0..100 range.
  virtual int GetProgress() const = 0;

  const std::string& uuid() const { return uuid_; }
  DiskImageStatus status() const { return status_; }
  const std::string& failure_reason() const { return failure_reason_; }

 protected:
  DiskImageOperation();

  void set_status(DiskImageStatus status) { status_ = status; }
  void set_failure_reason(const std::string& reason) {
    failure_reason_ = reason;
  }

 private:
  // UUID assigned to the operation.
  const std::string uuid_;

  // Status of the operation.
  DiskImageStatus status_;

  // Failure reason, if any, to be communicated to the callers.
  std::string failure_reason_;

  DISALLOW_COPY_AND_ASSIGN(DiskImageOperation);
};

struct ArchiveReadDeleter {
  void operator()(struct archive* in) { archive_read_free(in); }
};
using ArchiveReader = std::unique_ptr<struct archive, ArchiveReadDeleter>;

struct ArchiveWriteDeleter {
  void operator()(struct archive* out) { archive_write_free(out); }
};
using ArchiveWriter = std::unique_ptr<struct archive, ArchiveWriteDeleter>;

class PluginVmImportOperation : public DiskImageOperation {
 public:
  static std::unique_ptr<PluginVmImportOperation> Create(
      base::ScopedFD in_fd,
      const base::FilePath& disk_path,
      uint64_t source_size);

  bool Run(uint64_t io_limit) override;
  int GetProgress() const override;

 private:
  PluginVmImportOperation(base::ScopedFD in_fd, uint64_t source_size);

  bool PrepareInput();
  bool PrepareOutput(const base::FilePath& disk_path);

  void MarkFailed(const char* msg, struct archive* a);

  // Copies up to |io_limit| bytes of VM image.
  bool CopyImage(uint64_t io_limit);

  // Copies up to |io_limit| bytes of one archive entry of the image.
  // Returns number of bytes read.
  uint64_t CopyEntry(uint64_t io_limit);

  // Called after copying all archive entries to commit imported image.
  void FinalizeCopy();

  // File descriptor from which to fetch the source image.
  base::ScopedFD in_fd_;

  // Size of the source image (compressed) as reported by the caller.
  uint64_t source_size_;

  // Number of bytes (compressed) fetched from the file descriptor so far.
  uint64_t processed_size_;

  // Progress level of the last time we told caller that its clients should
  // be notified of progress being made.
  int last_report_level_;

  // We are in a middle of copying an archive entry. Copying of one archive
  // entry may span several Run() invocations, depending on the size of the
  // entry.
  bool copying_data_;

  // Destination directory object.
  base::ScopedTempDir output_dir_;

  // Input compressed archive backed up by the file descriptor.
  ArchiveReader in_;

  // "Archive" representing output uncompressed directory.
  ArchiveWriter out_;

  DISALLOW_COPY_AND_ASSIGN(PluginVmImportOperation);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_DISK_IMAGE_H_
