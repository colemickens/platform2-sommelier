// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shims/environment.h"

#include <cstdlib>
#include <unistd.h>

using std::map;
using std::string;

namespace shill {

namespace shims {

Environment::Environment() = default;

Environment::~Environment() = default;

// static
Environment* Environment::GetInstance() {
  static base::NoDestructor<Environment> instance;
  return instance.get();
}

bool Environment::GetVariable(const string& name, string* value) {
  char* v = getenv(name.c_str());
  if (v) {
    *value = v;
    return true;
  }
  return false;
}

map<string, string> Environment::AsMap() {
  map<string, string> env;
  for (char** var = environ; var && *var; var++) {
    string v = *var;
    size_t assign = v.find('=');
    if (assign != string::npos) {
      env[v.substr(0, assign)] = v.substr(assign + 1);
    }
  }
  return env;
}

}  // namespace shims

}  // namespace shill
