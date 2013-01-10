// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xutil.h>

#include "egl_stuff.h"
#include "synccontroltest.h"

#if !defined(KHRONOS_SUPPORT_INT64)
#error int64 not defined by Khronos :-(
#endif

namespace {

#ifndef EGL_CHROMIUM_sync_controls
#define EGL_CHROMIUM_sync_controls 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETSYNCVALUESCHROMIUMPROC)(
    EGLDisplay dpy,
    EGLSurface surface,
    khronos_uint64_t* ust,
    khronos_uint64_t* msc,
    khronos_uint64_t* sbc);
#endif

struct SyncValues {
  khronos_uint64_t ust;
  khronos_uint64_t msc;
  khronos_uint64_t sbc;
};

const int kAcceptableError = 250;  // uS
const float kFillValueRed = 1.0;
const float kFillValueGreen = 0.0;
const float kFillValueBlue = 0.0;
}  // namespace

class EGLSyncControlTest : public SyncControlTest {
 public:
  EGLSyncControlTest();
  ~EGLSyncControlTest();

  virtual void Init();
  virtual bool Loop(int interval);

 private:
  EGLInterface* interface_;
  PFNEGLGETSYNCVALUESCHROMIUMPROC egl_get_sync_values_;

  struct SyncValues GetSyncValues();
};

SyncControlTest* SyncControlTest::Create() {
  return new EGLSyncControlTest();
}

EGLSyncControlTest::EGLSyncControlTest()
    : interface_(static_cast<EGLInterface*>(g_main_gl_interface.get())),
      egl_get_sync_values_(NULL) { }

EGLSyncControlTest::~EGLSyncControlTest() { }

void EGLSyncControlTest::Init() {
  // Make sure that extension under test is present and usable
  std::string extensions(" ");
  extensions += eglQueryString(interface_->display(), EGL_EXTENSIONS);
  extensions += " ";
  std::string name(" EGL_CHROMIUM_sync_control ");
  CHECK(extensions.find(name) != std::string::npos)
      << "Extension EGL_CHROMIUM_sync_control not available!";

  egl_get_sync_values_ = reinterpret_cast<PFNEGLGETSYNCVALUESCHROMIUMPROC>(
      eglGetProcAddress("eglGetSyncValuesCHROMIUM"));
  CHECK(egl_get_sync_values_)
      << "Function eglGetSyncValuesCHROMIUM is not available!";
}

bool EGLSyncControlTest::Loop(int interval) {
  struct SyncValues first_call, second_call;
  khronos_uint64_t real_ust_delta, expected_ust_delta, ust_delta_error;

  glClearColor(kFillValueRed, kFillValueGreen, kFillValueBlue, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);

  first_call = GetSyncValues();
  interface_->SwapBuffers();
  usleep(interval);
  second_call = GetSyncValues();

  bool test_val = true;
  if (first_call.ust >= second_call.ust) {
    LOG(ERROR) << "Failure: First ust value is equal to or greater then the "
               << "second!";
    test_val = false;
  }

  if (first_call.msc >= second_call.msc) {
    LOG(ERROR) << "Failure: First msc value is equal to or greater then the "
               << "second!";
    test_val = false;
  }

  if (first_call.sbc >= second_call.sbc) {
    LOG(ERROR) << "Failure: First sbc value is equal to or greater then the "
               << "second!";
    test_val = false;
  }

  real_ust_delta = second_call.ust - first_call.ust;
  expected_ust_delta = (second_call.msc - first_call.msc) * interval;
  ust_delta_error = (real_ust_delta > expected_ust_delta) ?
      (real_ust_delta - expected_ust_delta) :
      (expected_ust_delta - real_ust_delta);
  if ((second_call.msc - first_call.msc) * kAcceptableError < ust_delta_error) {
    LOG(ERROR) << "Failure: ust delta is not within acceptable error "
               << "(" << (second_call.ust - first_call.ust) << "instead of "
               << (second_call.msc - first_call.msc) * interval << ")!";
    test_val = false;
  }

  if (!test_val) {
    LOG(ERROR) << "First call to eglGetSyncValuesCHROMIUM returned:\n\t"
               << "ust=" << first_call.ust << "\n\t"
               << "msc=" << first_call.msc << "\n\t"
               << "sbc=" << first_call.sbc;
    LOG(ERROR) << "Second call to eglGetSyncValuesCHROMIUM returned:\n\t"
               << "ust=" << second_call.ust << "\n\t"
               << "msc=" << second_call.msc << "\n\t"
               << "sbc=" << second_call.sbc;
  }
  return test_val;
}

struct SyncValues EGLSyncControlTest::GetSyncValues() {
  struct SyncValues ret_val;
  egl_get_sync_values_(interface_->display(),
                       interface_->surface(),
                       &ret_val.ust,
                       &ret_val.msc,
                       &ret_val.sbc);
  interface_->CheckError();
  return ret_val;
}
