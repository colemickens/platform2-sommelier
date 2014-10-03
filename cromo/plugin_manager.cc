// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cromo/plugin_manager.h"

#include <dirent.h>
#include <dlfcn.h>

#include <cerrno>
#include <cstring>

#include <base/logging.h>

#ifndef PLUGINDIR
#define PLUGINDIR "./plugins"
#endif

using std::string;
using std::vector;

vector<PluginManager::Plugin*> PluginManager::loaded_plugins_;

void PluginManager::LoadPlugins(CromoServer* server, const string& plugins) {
  DIR* pdir = opendir(PLUGINDIR);
  struct dirent* dirent;

  if (!pdir) {
    PLOG(ERROR) << "Cannot open plugin directory " PLUGINDIR;
    return;
  }

  while ((dirent = readdir(pdir)) != nullptr) {
    string leafname(dirent->d_name);
    if (leafname.length() < 3 ||
        leafname.substr(leafname.length()-3, string::npos) != ".so")
      continue;

    string filename(PLUGINDIR);
    filename.append("/").append(leafname);
    void* handle = dlopen(filename.c_str(), RTLD_LAZY);
    if (!handle) {
      LOG(ERROR) << "Cannot load plugin: " << dlerror();
      continue;
    }
    cromo_plugin_descriptor* pdesc = reinterpret_cast<cromo_plugin_descriptor*>(
        dlsym(handle, "plugin_descriptor"));
    if (!pdesc) {
      LOG(ERROR) << "Plugin does not contain descriptor: " << dlerror();
      dlclose(handle);
      continue;
    }
    if (plugins.size() == 0 ||
        (pdesc->name && plugins.find(pdesc->name) != string::npos)) {
      Plugin* pl = new Plugin;
      pl->handle = handle;
      pl->descriptor = pdesc;
      pl->initted = false;
      loaded_plugins_.push_back(pl);
      LOG(INFO) << "Loaded plugin " << pdesc->name;
    }
  }
  closedir(pdir);

  vector<Plugin*>::const_iterator it;
  for (it = loaded_plugins_.begin(); it != loaded_plugins_.end(); ++it) {
    Plugin* pl = *it;
    if (pl->descriptor->onload) {
      pl->descriptor->onload(server);
      pl->initted = true;
    }
  }
}

void PluginManager::UnloadPlugins(bool dlclose_plugins) {
  vector<Plugin*>::const_iterator it;
  for (it = loaded_plugins_.begin(); it != loaded_plugins_.end(); ++it) {
    Plugin* pl = *it;
    if (pl->initted && pl->descriptor->onunload) {
      pl->descriptor->onunload();
    }
  }

  // We do not always dlclose plugins if the process is about to exit
  // anyway.  Critical cleanup has already happpened by calling the
  // onunload functions.
  if (dlclose_plugins) {
    for (it = loaded_plugins_.begin(); it != loaded_plugins_.end(); ++it) {
      Plugin* pl = *it;
      dlclose(pl->handle);
    }
  }
  loaded_plugins_.erase(loaded_plugins_.begin(), loaded_plugins_.end());
}
