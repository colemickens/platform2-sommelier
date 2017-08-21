// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_FAKE_SAMBA_INTERFACE_H_
#define SMBPROVIDER_FAKE_SAMBA_INTERFACE_H_

#include <base/files/file_path.h>
#include "base/compiler_specific.h"

#include "smbprovider/samba_interface.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace smbprovider {

// Fake implementation of SambaInterface. Uses a map to simulate a fake file
// system that can open and close directories. It can also store entries through
// |FakeEntry| which hold entry metadata.
class FakeSambaInterface : public SambaInterface {
 public:
  FakeSambaInterface();
  ~FakeSambaInterface() {}

  int32_t OpenDirectory(const std::string& directory_path,
                        int32_t* dir_id) override;

  int32_t CloseDirectory(int32_t dir_id) override;

  int32_t GetDirectoryEntries(int32_t dir_id,
                              smbc_dirent* dirp,
                              int32_t dirp_buffer_size,
                              int32_t* bytes_read) override;

  int32_t GetEntryStatus(const std::string& entry_path,
                         struct stat* stat) override;

  // Adds a directory that is able to be opened through OpenDirectory() by
  // adding it to |directory_map_|. This also constructs the corresponding
  // vector value in |entry_map_|.
  // |path| is the directory path.
  // Returns the directory id.
  int32_t AddDirectory(const std::string& path);

  // Adds a FakeEntry in entry_map_ with |name| as the key.
  // |dir_id| is the id returned by AddDirectory().
  // |name| is the name of the entry.
  // |smbc_type| is the type of entry.
  // |size| is the size of the entry.
  void AddEntry(int32_t dir_id,
                const std::string& name,
                uint32_t smbc_type,
                size_t size);

  // Helper method to check if there are any leftover open directories in
  // |directory_map_|.
  bool HasOpenDirectories() const;

 private:
  // Replacement struct for smbc_dirent within FakeSambaInterface.
  struct FakeEntry {
    std::string name;
    uint32_t smbc_type;
    size_t size;

    FakeEntry(const std::string& entry_name, uint32_t type, size_t size)
        : name(entry_name), smbc_type(type), size(size) {}
  };

  typedef std::map<std::string, FakeEntry> EntryMap;

  // This is used in |directory_map_| and is used as a fake file system.
  struct FakeDirectory {
    int32_t dir_id;
    std::string path;
    // Whether or not the current directory is currently opened through
    // OpenDirectory().
    bool is_open;
    // Contains entries that can be found in this directory. It is a mapping of
    // entry name to its |FakeEntry|.
    EntryMap entries;
    // Keeps track of the current key from |entries| that is being read from
    // GetDirectoryEntries(). It will reset to the key of the first entry in
    // |entries| when OpenDirectory() is called. On CloseDirectory(), this will
    // be set to empty string.
    std::string current_entry;

    FakeDirectory(int32_t dir_id, const std::string& path)
        : dir_id(dir_id), path(path), is_open(false) {}

    // Finds an entry in |entries| based on its |full_path|.
    // Returns entries.end() if the path does not match, or if the |full_path|
    // is not found.
    EntryMap::const_iterator FindEntryByFullPath(
        const base::FilePath& full_path) const;

    bool HasMoreEntries() const {
      return entries.find(current_entry) != entries.end();
    }
  };

  // Assigned as a directory id to a directory that is opened in
  // OpenDirectory(). It is then incremented afterwards.
  int dir_id_counter_ = 0;

  // Keeps track of the directories that are able to be opened through
  // OpenDirectory().
  // Key: Directory ID of the share/directory.
  // Value: FakeDirectory that corresponds with the key.
  std::map<int32_t, FakeDirectory> directory_map_;
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_FAKE_SAMBA_INTERFACE_H_
