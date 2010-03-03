// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BENCH_GL_YUV2RGB_H_
#define BENCH_GL_YUV2RGB_H_

#define YUV2RGB_SHADER "yuv2rgb.glslf"

#define YUV2RGB_NAME "image.yuv"
#define YUV2RGB_WIDTH 720
// YUV2RGB_HEIGHT is total height, which is 3/2 of height of Y plane.
#define YUV2RGB_HEIGHT 729
#define YUV2RGB_SIZE (YUV2RGB_WIDTH * YUV2RGB_HEIGHT)

#endif
