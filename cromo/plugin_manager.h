// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_PLUGIN_MANAGER_H_
#define CROMO_PLUGIN_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "plugin.h"

class CromoServer;

class PluginManager {
 public:
  static void LoadPlugins(CromoServer* server, std::string& plugins);
  static void UnloadPlugins(bool dlclose_plugins);

 private:
  struct Plugin {
    void* handle;
    cromo_plugin_descriptor* descriptor;
    bool initted;
  };
  static std::vector<Plugin*> loaded_plugins_;

  DISALLOW_COPY_AND_ASSIGN(PluginManager);
};


#endif  // CROMO_PLUGIN_MANAGER_H_
