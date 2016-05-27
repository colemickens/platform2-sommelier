// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGELOADER_IMAGELOADER_COMMON_H_
#define IMAGELOADER_IMAGELOADER_COMMON_H_

namespace imageloader {

const char kBadResult[] = "";
const char kImageLoaderName[] = "org.chromium.ImageLoader";
const char kImageLoaderPath[] = "/org/chromium/ImageLoader";

void OnQuit(int sig);

}  // namespace imageloader

#endif  // IMAGELOADER_IMAGELOADER_COMMON_H_
