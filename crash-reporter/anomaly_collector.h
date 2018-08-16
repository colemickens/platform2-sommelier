// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_ANOMALY_COLLECTOR_H_
#define CRASH_REPORTER_ANOMALY_COLLECTOR_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int AnomalyLexer(bool flag_filter, bool flag_test);

#ifdef __cplusplus
}
#endif

#endif  // CRASH_REPORTER_ANOMALY_COLLECTOR_H_
