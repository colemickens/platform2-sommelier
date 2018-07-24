// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_GLIB_H_
#define SHILL_MOCK_GLIB_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/glib.h"

namespace shill {

class MockGLib : public GLib {
 public:
  MockGLib();
  ~MockGLib() override;

  MOCK_METHOD3(ChildWatchAdd, guint(GPid pid,
                                    GChildWatchFunc function,
                                    gpointer data));
  MOCK_METHOD1(SourceRemove, gboolean(guint tag));
  MOCK_METHOD8(SpawnAsync, gboolean(const gchar* working_directory,
                                    gchar** argv,
                                    gchar** envp,
                                    GSpawnFlags flags,
                                    GSpawnChildSetupFunc child_setup,
                                    gpointer user_data,
                                    GPid* child_pid,
                                    GError** error));
  MOCK_METHOD0(TypeInit, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGLib);
};

}  // namespace shill

#endif  // SHILL_MOCK_GLIB_H_
