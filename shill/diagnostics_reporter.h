// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DIAGNOSTICS_REPORTER_H_
#define SHILL_DIAGNOSTICS_REPORTER_H_

#include <base/lazy_instance.h>

namespace shill {

class GLib;

class DiagnosticsReporter {
 public:
  virtual ~DiagnosticsReporter();

  // This is a singleton -- use DiagnosticsReporter::GetInstance()->Foo()
  static DiagnosticsReporter *GetInstance();

  void Init(GLib *glib);

  // Handle a connectivity event -- collect and stash diagnostics data, possibly
  // uploading it for analysis.
  virtual void OnConnectivityEvent();

 protected:
  DiagnosticsReporter();

  virtual bool IsReportingEnabled();

  // Uploads diagnostics data for analysis.
  void Report();

 private:
  friend struct base::DefaultLazyInstanceTraits<DiagnosticsReporter>;
  friend class DiagnosticsReporterTest;

  GLib *glib_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsReporter);
};

}  // namespace shill

#endif  // SHILL_DIAGNOSTICS_REPORTER_H_
