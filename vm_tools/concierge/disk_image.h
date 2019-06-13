// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_DISK_IMAGE_H_
#define VM_TOOLS_CONCIERGE_DISK_IMAGE_H_

#include <archive.h>

#include <memory>
#include <string>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <dbus/exported_object.h>
#include <dbus/object_proxy.h>

#include <vm_concierge/proto_bindings/service.pb.h>

#include "vm_tools/common/vm_id.h"

namespace vm_tools {
namespace concierge {

class DiskImageOperation {
 public:
  virtual ~DiskImageOperation() = default;

  // Execute next chunk of the disk operation, handling up to |io_limit| bytes.
  void Run(uint64_t io_limit);

  // Report operation progress, in 0..100 range.
  int GetProgress() const;

  const std::string& uuid() const { return uuid_; }
  DiskImageStatus status() const { return status_; }
  const std::string& failure_reason() const { return failure_reason_; }
  int64_t processed_size() const { return processed_size_; }

 protected:
  DiskImageOperation();

  // Executes up to |io_limit| bytes of disk operation.
  virtual bool ExecuteIo(uint64_t io_limit) = 0;

  // Called after all IO is done to commit the result.
  virtual void Finalize() = 0;

  void AccumulateProcessedSize(uint64_t size) { processed_size_ += size; }

  void set_status(DiskImageStatus status) { status_ = status; }
  void set_failure_reason(const std::string& reason) {
    failure_reason_ = reason;
  }
  void set_source_size(uint64_t source_size) { source_size_ = source_size; }

 private:
  // UUID assigned to the operation.
  const std::string uuid_;

  // Status of the operation.
  DiskImageStatus status_;

  // Failure reason, if any, to be communicated to the callers.
  std::string failure_reason_;

  // Size of the source of disk operation (bytes).
  uint64_t source_size_;

  // Number of bytes consumed from the source.
  uint64_t processed_size_;

  DISALLOW_COPY_AND_ASSIGN(DiskImageOperation);
};

class PluginVmCreateOperation : public DiskImageOperation {
 public:
  static std::unique_ptr<PluginVmCreateOperation> Create(
      base::ScopedFD in_fd,
      const base::FilePath& iso_dir,
      uint64_t source_size,
      const VmId vm_id,
      const std::vector<std::string> params);

 protected:
  bool ExecuteIo(uint64_t io_limit) override;
  void Finalize() override;

 private:
  PluginVmCreateOperation(base::ScopedFD in_fd,
                          uint64_t source_size,
                          const VmId vm_id_,
                          const std::vector<std::string> params);

  bool PrepareOutput(const base::FilePath& iso_dir);

  void MarkFailed(const char* msg, int error_code);

  // VM owner and name. Used when registering imported image with the
  // dispatcher.
  const VmId vm_id_;

  // Parameters that need to be passed to the Plugin VM helper when
  // creating the VM.
  const std::vector<std::string> params_;

  // File descriptor from which to fetch the source image.
  base::ScopedFD in_fd_;

  // File descriptor to where the data from source image will
  // be written to.
  base::ScopedFD out_fd_;

  // Destination directory object.
  base::ScopedTempDir output_dir_;

  DISALLOW_COPY_AND_ASSIGN(PluginVmCreateOperation);
};

struct ArchiveReadDeleter {
  void operator()(struct archive* in) { archive_read_free(in); }
};
using ArchiveReader = std::unique_ptr<struct archive, ArchiveReadDeleter>;

struct ArchiveWriteDeleter {
  void operator()(struct archive* out) { archive_write_free(out); }
};
using ArchiveWriter = std::unique_ptr<struct archive, ArchiveWriteDeleter>;

class PluginVmExportOperation : public DiskImageOperation {
 public:
  static std::unique_ptr<PluginVmExportOperation> Create(
      const VmId vm_id, const base::FilePath disk_path, base::ScopedFD out_fd);

 protected:
  bool ExecuteIo(uint64_t io_limit) override;
  void Finalize() override;

 private:
  PluginVmExportOperation(const VmId vm_id,
                          const base::FilePath disk_path,
                          base::ScopedFD out_fd);

  bool PrepareInput();
  bool PrepareOutput();

  void MarkFailed(const char* msg, struct archive* a);

  // Copies up to |io_limit| bytes of one file of the image.
  // Returns number of bytes read.
  uint64_t CopyEntry(uint64_t io_limit);

  // VM owner and name.
  const VmId vm_id_;

  // Path to the directory containing source image.
  const base::FilePath src_image_path_;

  // File descriptor to write the compressed image to.
  base::ScopedFD out_fd_;

  // We are in a middle of copying an archive entry. Copying of one archive
  // entry may span several Run() invocations, depending on the size of the
  // entry.
  bool copying_data_;

  // Source directory "archive".
  ArchiveReader in_;

  // Output archive backed by the file descriptor.
  ArchiveWriter out_;

  DISALLOW_COPY_AND_ASSIGN(PluginVmExportOperation);
};

class PluginVmImportOperation : public DiskImageOperation {
 public:
  static std::unique_ptr<PluginVmImportOperation> Create(
      base::ScopedFD in_fd,
      const base::FilePath disk_path,
      uint64_t source_size,
      const VmId vm_id,
      dbus::ObjectProxy* vmplugin_service_proxy);

 protected:
  bool ExecuteIo(uint64_t io_limit) override;
  void Finalize() override;

 private:
  PluginVmImportOperation(base::ScopedFD in_fd,
                          uint64_t source_size,
                          const base::FilePath disk_path,
                          const VmId vm_id_,
                          dbus::ObjectProxy* vmplugin_service_proxy);

  bool PrepareInput();
  bool PrepareOutput();

  void MarkFailed(const char* msg, struct archive* a);

  // Copies up to |io_limit| bytes of one archive entry of the image.
  // Returns number of bytes read.
  uint64_t CopyEntry(uint64_t io_limit);

  // Path to the directory that will contain the imported image.
  const base::FilePath dest_image_path_;

  // VM owner and name. Used when registering imported image with the
  // dispatcher.
  const VmId vm_id_;

  // Proxy to the dispatcher service.  Not owned.
  dbus::ObjectProxy* vmplugin_service_proxy_;

  // File descriptor from which to fetch the source image.
  base::ScopedFD in_fd_;

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
