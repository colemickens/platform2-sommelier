// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_SETUP_ARC_READ_AHEAD_FILES_NYC_H_
#define ARC_SETUP_ARC_READ_AHEAD_FILES_NYC_H_

#include <array>

namespace arc {

// A list of files that should be fully read. The list is manually created based
// on an output of
//    localhost ~ # ureadahead --dump /opt/google/containers/android/rootfs/root
// on veyron_minnie 9957.0.0. The files in the list meet all of the following:
//  * Larger than 500KB.
//  * More than ~80% of the file is read.
//  * The mini instance does not load the file.
constexpr std::array<const char* const, 44> kImportantFiles{{
    "Bluetooth.apk",
    "Bluetooth.odex",
    "DocumentsUI.apk",
    "DownloadProvider.apk",
    "ManagedProvisioning.apk",
    "PrebuiltGmsCoreRelease.apk",
    "PrintSpooler.apk",
    "Settings.apk",
    "Settings.odex",
    "SettingsProvider.odex",
    "StorageManager.apk",
    "SystemUI.apk",
    "SystemUI.odex",
    "TelephonyProvider.apk",
    "TelephonyProvider.odex",
    "framework-res.apk",
    "icudt56l.dat",
    "libLLVM.so",
    "libandroid_runtime.so",
    "libarcbridge.so",
    "libart-compiler.so",
    "libart.so",
    "libaudioflinger.so",
    "libcameraservice.so",
    "libchrome.so",
    "libcrypto.so",
    "libdng_sdk.so",
    "libft2.so",
    "libgui.so",
    "libhwui.so",
    "libicui18n.so",
    "libicuuc.so",
    "libmedia.so",
    "libmediaplayerservice.so",
    "libmojo.so",
    "libskia.so",
    "libsqlite.so",
    "libstagefright.so",
    "libstagefright_soft_aacenc.so",
    "libstagefright_soft_hevcdec.so",
    "libwayland_service.so",
    "services.odex",
    "tzdata",
    "wifi-service.odex",
}};

// A list of files that should be read up to |kDefaultReadAheadSize| bytes.
constexpr std::array<const char* const, 2> kImportantExtensions{
    {".so", ".ttf"}};

}  // namespace arc

#endif  // ARC_SETUP_ARC_READ_AHEAD_FILES_NYC_H_
