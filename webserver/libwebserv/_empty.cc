// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is left empty deliberately to work around some dependency
// generation in GYP. In GYP for custom actions to run properly, the 'sources'
// section must contain at least one recognizable source file. Since libwebserv
// inherits all its source from libwebserv_common target and has no additional
// C++ source files, this file is used for this purpose, even though it doesn't
// produce any compiled code.
