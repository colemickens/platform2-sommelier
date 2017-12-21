// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_FAKE_SAMBA_INTERFACE_H_
#define SMBPROVIDER_FAKE_SAMBA_INTERFACE_H_

#include <base/files/file_path.h>
#include "base/compiler_specific.h"

#include "smbprovider/samba_interface.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace smbprovider {

// Fake implementation of SambaInterface. Uses a map to simulate a fake file
// system that can open and close directories. It can also store entries through
// |FakeEntry| which hold entry metadata.
class FakeSambaInterface : public SambaInterface {
 public:
  FakeSambaInterface();
  ~FakeSambaInterface() {}

  // SambaInterface overrides.
  int32_t OpenDirectory(const std::string& directory_path,
                        int32_t* dir_id) override;

  int32_t CloseDirectory(int32_t dir_id) override;

  int32_t GetDirectoryEntries(int32_t dir_id,
                              smbc_dirent* dirp,
                              int32_t dirp_buffer_size,
                              int32_t* bytes_read) override;

  int32_t GetEntryStatus(const std::string& entry_path,
                         struct stat* stat) override;

  // Adds a directory that is able to be opened through OpenDirectory().
  // Does not support recursive creation. All parents must exist.
  void AddDirectory(const std::string& path);

  // Adds a file at the specified path. All parents must exist.
  void AddFile(const std::string& path);
  void AddFile(const std::string& path, size_t size);
  void AddFile(const std::string& path, size_t size, uint64_t date);

  void AddEntry(const std::string& path, uint32_t smbc_type);

  // Helper method to check if there are any leftover open directories or files
  // in |open_fds|.
  bool HasOpenEntries() const;

 private:
  // Replacement struct for smbc_dirent within FakeSambaInterface.
  struct FakeEntry {
    std::string name;
    uint32_t smbc_type;
    size_t size;
    uint64_t date;

    FakeEntry(const std::string& full_path,
              uint32_t smbc_type,
              size_t size,
              uint64_t date);

    virtual ~FakeEntry() = default;

    // Returns true for SMBC_FILE and SMBC_DIR. False for all others.
    bool IsValidEntryType() const;

    DISALLOW_COPY_AND_ASSIGN(FakeEntry);
  };

  // This is used in |directory_map_| and is used as a fake file system.
  struct FakeDirectory : FakeEntry {
    explicit FakeDirectory(const std::string& full_path)
        : FakeEntry(full_path, SMBC_DIR, 0 /* size */, 0 /* date */) {}

    // Returns a pointer to the entry in the directory with |name|.
    FakeEntry* FindEntry(const std::string& name);

    // Contains pointers to entries that can be found in this directory.
    std::vector<std::unique_ptr<FakeEntry>> entries;

    DISALLOW_COPY_AND_ASSIGN(FakeDirectory);
  };

  struct FakeFile : FakeEntry {
    // TODO(allenvic): Add member to represent file data.

    FakeFile(const std::string& full_path, size_t size, uint64_t date)
        : FakeEntry(full_path, SMBC_FILE, size, date) {}

    DISALLOW_COPY_AND_ASSIGN(FakeFile);
  };

  struct OpenInfo {
    std::string full_path;

    // Keeps track of the index of the next file to be read from an
    // open directory. Set to 0 when opening a directory.
    size_t current_entry = 0;

    // For testing that read/write are set correctly by Open().
    bool readable = false;
    bool writeable = false;

    explicit OpenInfo(const std::string& full_path) : full_path(full_path) {}

    OpenInfo(OpenInfo&& other)
        : full_path(std::move(other.full_path)),
          current_entry(other.current_entry),
          readable(other.readable),
          writeable(other.writeable) {}

    DISALLOW_COPY_AND_ASSIGN(OpenInfo);
  };

  using OpenEntries = std::map<uint32_t, OpenInfo>;
  using OpenEntriesIterator = OpenEntries::iterator;

  // Checks whether the file/directory at the specified path is open.
  bool IsOpen(const std::string& full_path) const;

  // Checks whether a file descriptor is open.
  bool IsFDOpen(uint32_t fd) const;

  // Returns an iterator to an OpenInfo in open_fds.
  OpenEntriesIterator FindOpenFD(uint32_t fd);

  // Recurses through the file system returning a pointer to a directory.
  // Pointer is owned by the class and should not be retained passed the
  // lifetime of a single public method call as it could be invalidated.
  FakeDirectory* GetDirectory(const std::string& full_path,
                              int32_t* error) const;
  FakeDirectory* GetDirectory(const std::string& full_path) const;

  // Recurses through the file system return a pointer to the entry.
  // Pointer is owned by the class and should not be retained passed the
  // lifetime of a single public method call as it could be invalidated.
  FakeEntry* GetEntry(const std::string& entry_path) const;

  // Checks whether the directory has more entries.
  bool HasMoreEntries(uint32_t dir_fd) const;

  // Counter for assigning file descriptor when opening.
  uint32_t next_fd = 0;

  // Root directory of the file system.
  std::unique_ptr<FakeDirectory> root;

  // Keeps track of open files and directories.
  // Key: fd of the file/directory.
  // Value: OpenInfo that corresponds with the key.
  OpenEntries open_fds;

  DISALLOW_COPY_AND_ASSIGN(FakeSambaInterface);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_FAKE_SAMBA_INTERFACE_H_
