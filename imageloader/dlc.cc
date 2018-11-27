// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "imageloader/dlc.h"

#include <set>

#include <base/files/file_util.h>
#include <chromeos/dbus/service_constants.h>

#include "imageloader/component.h"
#include "imageloader/manifest.h"

namespace imageloader {

namespace {

// The path to DLC root folder in rootfs;
constexpr char kRootPathRootfs[] = "/opt/google/dlc/";
// The path to DLC root folder in stateful partition;
constexpr char kRootPathStateful[] = "/home/chronos/dlc/";
// The name of the image.
constexpr char kImageName[] = "dlc.img";

base::FilePath GetManifestPath(const std::string& id) {
  return base::FilePath(kRootPathRootfs).Append(id).Append("imageloader.json");
}

base::FilePath GetTablePath(const std::string& id) {
  return base::FilePath(kRootPathRootfs).Append(id).Append("table");
}

base::FilePath GetImagePath(const std::string& id, AOrB a_or_b) {
  base::FilePath root = base::FilePath(kRootPathStateful).Append(id);
  if (a_or_b == AOrB::kDlcA) {
    return root.Append("dlc_a").Append(kImageName);
  } else if (a_or_b == AOrB::kDlcB) {
    return root.Append("dlc_b").Append(kImageName);
  } else {
    return base::FilePath();
  }
}

AOrB GetImageAOrB(const std::string& a_or_b) {
  if (a_or_b == imageloader::kSlotNameA) {
    return AOrB::kDlcA;
  } else if (a_or_b == imageloader::kSlotNameB) {
    return AOrB::kDlcB;
  } else {
    return AOrB::kUnknown;
  }
}

}  // namespace

Dlc::Dlc(const std::string& id) : id_(id) {}

bool Dlc::Mount(HelperProcessProxy* proxy,
                const std::string& a_or_b_str,
                const base::FilePath& mount_point) {
  AOrB a_or_b = GetImageAOrB(a_or_b_str);

  if (a_or_b == AOrB::kUnknown) {
    LOG(ERROR) << "Unknown image type: " << a_or_b_str;
    return false;
  }

  return Mount(proxy, GetImagePath(id_, a_or_b), GetManifestPath(id_),
               GetTablePath(id_), a_or_b, mount_point);
}

bool Dlc::Mount(HelperProcessProxy* proxy,
                const base::FilePath& image_path,
                const base::FilePath& manifest_path,
                const base::FilePath& table_path,
                AOrB a_or_b,
                const base::FilePath& mount_point) {
  std::string manifest_raw;
  if (!base::ReadFileToStringWithMaxSize(manifest_path, &manifest_raw,
                                         kMaximumFilesize)) {
    LOG(ERROR) << "Could not read manifest file." << manifest_path.value();
    return false;
  }
  Manifest manifest;
  if (!manifest.ParseManifest(manifest_raw))
    return false;

  std::string table;
  if (!base::ReadFileToStringWithMaxSize(table_path, &table,
                                         kMaximumFilesize)) {
    LOG(ERROR) << "Could not read table.";
    return false;
  }
  base::File image(image_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!image.IsValid()) {
    LOG(ERROR) << "Could not open image file '" << image_path.value() << "': "
               << base::File::ErrorToString(image.error_details());
    return false;
  }
  base::ScopedFD image_fd(image.TakePlatformFile());

  return proxy->SendMountCommand(image_fd.get(), mount_point.value(),
                                 manifest.fs_type(), table);
}

}  // namespace imageloader
