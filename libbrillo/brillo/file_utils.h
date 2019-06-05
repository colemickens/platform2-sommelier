// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_FILE_UTILS_H_
#define LIBBRILLO_BRILLO_FILE_UTILS_H_

#include <sys/types.h>

#include <string>

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <brillo/brillo_export.h>
#include <brillo/secure_blob.h>

namespace brillo {

// Ensures a regular file owned by user |uid| and group |gid| exists at |path|.
// Any other entity at |path| will be deleted and replaced with an empty
// regular file. If a new file is needed, any missing parent directories will
// be created, and the file will be assigned |new_file_permissions|.
// Should be safe to use in all directories, including tmpdirs with the sticky
// bit set.
// Returns true if the file existed or was able to be created.
BRILLO_EXPORT bool TouchFile(const base::FilePath& path,
                             int new_file_permissions,
                             uid_t uid,
                             gid_t gid);

// Convenience version of TouchFile() defaulting to 600 permissions and the
// current euid/egid.
// Should be safe to use in all directories, including tmpdirs with the sticky
// bit set.
BRILLO_EXPORT bool TouchFile(const base::FilePath& path);

// Opens the absolute |path| to a regular file or directory ensuring that none
// of the path components are symbolic links and returns a FD. If |path| is
// relative, or contains any symbolic links, or points to a non-regular file or
// directory, an invalid FD is returned instead. |mode| is ignored unless
// |flags| has either O_CREAT or O_TMPFILE. Note that O_CLOEXEC is set so the
// file descriptor will not be inherited across exec calls.
//
// Parameters
//  path - An absolute path of the file to open
//  flags - Flags to pass to open.
//  mode - Mode to pass to open.
BRILLO_EXPORT base::ScopedFD OpenSafely(const base::FilePath& path,
                                        int flags,
                                        mode_t mode);

// Opens the |path| relative to the |parent_fd| to a regular file or directory
// ensuring that none of the path components are symbolic links and returns a
// FD. If |path| contains any symbolic links, or points to a non-regular file or
// directory, an invalid FD is returned instead. |mode| is ignored unless
// |flags| has either O_CREAT or O_TMPFILE. Note that O_CLOEXEC is set so the
// file descriptor will not be inherited across exec calls.
//
// Parameters
//  parent_fd - The file descriptor of the parent directory
//  path - An absolute path of the file to open
//  flags - Flags to pass to open.
//  mode - Mode to pass to open.
BRILLO_EXPORT base::ScopedFD OpenAtSafely(int parent_fd,
                                          const base::FilePath& path,
                                          int flags,
                                          mode_t mode);

// Opens the absolute |path| to a FIFO ensuring that none of the path components
// are symbolic links and returns a FD. If |path| is relative, or contains any
// symbolic links, or points to a non-regular file or directory, an invalid FD
// is returned instead. |mode| is ignored unless |flags| has either O_CREAT or
// O_TMPFILE.
//
// Parameters
//  path - An absolute path of the file to open
//  flags - Flags to pass to open.
//  mode - Mode to pass to open.
BRILLO_EXPORT base::ScopedFD OpenFifoSafely(const base::FilePath& path,
                                            int flags,
                                            mode_t mode);

// Iterates through the path components and creates any missing ones. Guarantees
// the ancestor paths are not symlinks. This function returns an invalid FD on
// failure. Newly created directories will have |mode| permissions. The returned
// file descriptor was opened with both O_RDONLY and O_CLOEXEC.
//
// Parameters
//  full_path - An absolute path of the directory to create and open.
BRILLO_EXPORT base::ScopedFD MkdirRecursively(const base::FilePath& full_path,
                                              mode_t mode);

// Writes the entirety of the given data to |path| with 0640 permissions
// (modulo umask).  If missing, parent (and parent of parent etc.) directories
// are created with 0700 permissions (modulo umask).  Returns true on success.
//
// Parameters
//  path      - Path of the file to write
//  blob/data - blob/string/array to populate from
// (size      - array size)
BRILLO_EXPORT bool WriteStringToFile(const base::FilePath& path,
                                     const std::string& data);
BRILLO_EXPORT bool WriteToFile(const base::FilePath& path,
                               const char* data,
                               size_t size);
template <class T>
BRILLO_EXPORT bool WriteBlobToFile(const base::FilePath& path, const T& blob) {
  return WriteToFile(path, reinterpret_cast<const char*>(blob.data()),
                     blob.size());
}

// Calls fdatasync() on file if data_sync is true or fsync() on directory or
// file when data_sync is false.  Returns true on success.
//
// Parameters
//   path - File/directory to be sync'ed
//   is_directory - True if |path| is a directory
//   data_sync - True if |path| does not need metadata to be synced
BRILLO_EXPORT bool SyncFileOrDirectory(const base::FilePath& path,
                                       bool is_directory,
                                       bool data_sync);

// Atomically writes the entirety of the given data to |path| with |mode|
// permissions (modulo umask).  If missing, parent (and parent of parent etc.)
// directories are created with 0700 permissions (modulo umask).  Returns true
// if the file has been written successfully and it has physically hit the
// disk.  Returns false if either writing the file has failed or if it cannot
// be guaranteed that it has hit the disk.
//
// Parameters
//   path - Path of the file to write
//   blob/data - blob/array to populate from
//   (size - array size)
//   mode - File permission bit-pattern, eg. 0644 for rw-r--r--
BRILLO_EXPORT bool WriteToFileAtomic(const base::FilePath& path,
                                     const char* data,
                                     size_t size,
                                     mode_t mode);
template <class T>
BRILLO_EXPORT bool WriteBlobToFileAtomic(const base::FilePath& path,
                                         const T& blob,
                                         mode_t mode) {
  return WriteToFileAtomic(path, reinterpret_cast<const char*>(blob.data()),
                           blob.size(), mode);
}

}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_FILE_UTILS_H_
