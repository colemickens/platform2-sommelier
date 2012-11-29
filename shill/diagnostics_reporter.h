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

  void Report();

 protected:
  DiagnosticsReporter();

  virtual bool IsReportingEnabled();

 private:
  friend struct base::DefaultLazyInstanceTraits<DiagnosticsReporter>;
  friend class DiagnosticsReporterTest;

  GLib *glib_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsReporter);
};

}  // namespace shill

#endif  // SHILL_DIAGNOSTICS_REPORTER_H_
