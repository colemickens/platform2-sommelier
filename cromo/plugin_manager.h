// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_PLUGIN_MANAGER_H_
#define CROMO_PLUGIN_MANAGER_H_

#include <vector>
#include "plugin.h"

class ModemManagerServer;

class PluginManager {
 public:
  static void LoadPlugins(ModemManagerServer* server, std::string& plugins);
  static void UnloadPlugins();

 private:
  struct Plugin {
    void* handle;
    cromo_plugin_descriptor* descriptor;
    bool initted;
  };
  static std::vector<Plugin*> loaded_plugins_;
};


#endif  // CROMO_PLUGIN_MANAGER_H_
