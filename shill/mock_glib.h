//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
