// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_HOOKTABLE_H_
#define CROMO_HOOKTABLE_H_

#include <map>
#include <string>

class HookTable {
  public:
    HookTable();
    ~HookTable();

    void Add(const std::string& name, bool (*func)(void *), void *arg);

    // Removes a hook from this table by name. The specified hook must exist in
    // the table.
    void Del(const std::string& name);

    // Executes all the hooks in the table in an undefined order. Returns
    // whether all hooks completed successfully. Hooks indicate success by
    // returning true and failure by returning false.
    bool Run();
  private:
    struct Hook {
      Hook(bool (*func)(void *), void *arg);
      ~Hook();
      bool (*func_)(void *arg);
      void *arg_;
    };
    typedef std::map<const std::string, Hook*> HookMap;
    HookMap hooks_;
};

#endif /* !CROMO_HOOKTABLE_H_ */
