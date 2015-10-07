// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/blob_store.h"

#include <algorithm>

#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "fides/file_utils.h"
#include "fides/key.h"

namespace fides {

namespace {

// Defines the maximum length of source ids. This requirement stems from the
// constraints on the maximum length of file system entries from the underlying
// operating system.
const unsigned int kMaxSourceIdLength = 255u;

// Defines the length of a blob filename. Note that this must be compatible with
// |kFormatBlobFilename|.
const unsigned int kBlobFilenameLength = 10u;

// Defines the file name format for blobs.
const char kFormatBlobFilename[] = "blob_%05u";

// Defines the path format of the directory for blobs belonging to a source.
const char kFormatSourcePath[] = "%s/%s/";

// Defines the maximum supported size of SettingsBlobs in bytes.
const unsigned int kMaxSettingsBlobSizeBytes = 1024u * 1024u;

}  // namespace

BlobStore::Handle::Handle() : blob_id_(0) {}

BlobStore::Handle::Handle(unsigned int blob_id, const std::string& source_id)
    : blob_id_(blob_id), source_id_(source_id) {}

bool BlobStore::Handle::IsValid() const {
  return blob_id_ != 0 && !source_id_.empty();
}

BlobStore::BlobStore(const std::string& storage_path)
    : storage_path_(storage_path) {
  DCHECK_EQ(base::StringPrintf(kFormatBlobFilename, 0).length(),
            kBlobFilenameLength);
}

BlobStore::Handle BlobStore::Store(const std::string& source_id,
                                   BlobRef blob) const {
  DCHECK(!source_id.empty());

  // Check if the directory for |source_id| exists, if it doesn't, create it.
  std::string source_path = GetSourcePath(source_id);
  if (source_path.empty())
    return Handle();

  if (!utils::PathExists(source_path))
    utils::CreateDirectory(source_path);

  // Determine the next unused blob id, construct the blob path and write blob.
  unsigned int blob_id = GetNextUnusedBlobId(source_id);
  std::string blob_path = GetBlobPath(blob_id, source_id);
  if (blob_path.empty())
    return Handle();
  if (utils::WriteFileAtomically(blob_path, blob.data(), blob.size()))
    return Handle(blob_id, source_id);

  // Failed to write the file. Return an invalid Handle.
  return Handle();
}

const std::vector<uint8_t> BlobStore::Load(Handle handle) const {
  std::vector<uint8_t> blob;
  std::string blob_path = GetBlobPath(handle.blob_id_, handle.source_id_);
  if (blob_path.empty())
    return std::vector<uint8_t>();
  utils::ReadFile(blob_path, &blob, kMaxSettingsBlobSizeBytes);
  return blob;
}

std::vector<BlobStore::Handle> BlobStore::List(
    const std::string& source_id) const {
  std::vector<BlobStore::Handle> handles;
  std::string source_path = GetSourcePath(source_id);
  if (source_path.empty())
    return std::vector<BlobStore::Handle>();
  std::vector<std::string> files = utils::ListFiles(source_path);
  for (auto& file : files) {
    Handle handle(FilenameToBlobId(file), source_id);
    if (handle.IsValid())
      handles.push_back(handle);
  }
  return handles;
}

bool BlobStore::Purge(Handle handle) const {
  if (!handle.IsValid())
    return false;
  std::string blob_path = GetBlobPath(handle.blob_id_, handle.source_id_);
  if (blob_path.empty())
    return false;
  return utils::DeleteFile(blob_path);
}

std::string BlobStore::GetBlobPath(unsigned int blob_id,
                                   const std::string& source_id) const {
  std::string source_path = GetSourcePath(source_id);
  if (source_path.empty())
    return std::string();
  std::string filename = base::StringPrintf(kFormatBlobFilename, blob_id);
  if (kBlobFilenameLength != filename.size()) {
    LOG(ERROR) << "Invalid blob id: " << blob_id;
    return std::string();
  }
  return source_path + filename;
}

std::string BlobStore::GetSourcePath(const std::string& source_id) const {
  if (source_id.empty() || source_id.length() > kMaxSourceIdLength ||
      !Key::IsValidKey(source_id)) {
    LOG(ERROR) << "Invalid source id: " << source_id;
    return std::string();
  }
  return base::StringPrintf(kFormatSourcePath, storage_path_.c_str(),
                            source_id.c_str());
}

unsigned int BlobStore::FilenameToBlobId(const std::string& filename) const {
  unsigned int id = 0;
  if (filename.size() != kBlobFilenameLength) {
    LOG(ERROR) << "Not a blob filename: " << filename;
    return id;
  }
  sscanf(filename.c_str(), kFormatBlobFilename, &id);
  return id;
}

unsigned int BlobStore::GetNextUnusedBlobId(
    const std::string& source_id) const {
  std::vector<std::string> files = utils::ListFiles(GetSourcePath(source_id));

  // Sort the filenames lexicographically and reverse iterate over them. Note
  // that due to the filename format <<kPrefix>>_XXXXX filenames lexicographic
  // sorting and blob id sorting are the same.
  std::sort(files.begin(), files.end());
  for (auto it = files.rbegin(); it != files.rend(); ++it) {
    // Attempt to extract the blob id from the filename.
    unsigned int id = FilenameToBlobId(*it);

    // If this file does not follow our naming scheme, go to next file.
    if (!id)
      continue;

    return id + 1;
  }

  // No previous blob for |source_id| found.
  return 1;
}

}  // namespace fides
