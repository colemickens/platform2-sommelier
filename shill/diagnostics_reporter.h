// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DIAGNOSTICS_REPORTER_H_
#define SHILL_DIAGNOSTICS_REPORTER_H_

#include <base/lazy_instance.h>
#include <base/memory/weak_ptr.h>

namespace shill {

class EventDispatcher;

class DiagnosticsReporter {
 public:
  virtual ~DiagnosticsReporter();

  // This is a singleton -- use DiagnosticsReporter::GetInstance()->Foo()
  static DiagnosticsReporter *GetInstance();

  void Init(EventDispatcher *dispatcher);

  void Report();

 protected:
  DiagnosticsReporter();

  virtual bool IsReportingEnabled();

 private:
  friend struct base::DefaultLazyInstanceTraits<DiagnosticsReporter>;
  friend class DiagnosticsReporterTest;

  void TriggerCrash();

  EventDispatcher *dispatcher_;
  base::WeakPtrFactory<DiagnosticsReporter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsReporter);
};

}  // namespace shill

#endif  // SHILL_DIAGNOSTICS_REPORTER_H_
