// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLUGIN_GOBI_3K_MODEM_HELPER_H_
#define PLUGIN_GOBI_3K_MODEM_HELPER_H_

#include "gobi_modem.h"

class Gobi3KModemHelper : public GobiModemHelper {
 public:
  Gobi3KModemHelper(gobi::Sdk* sdk) : GobiModemHelper(sdk) { };
  ~Gobi3KModemHelper() { };

  virtual void SetCarrier(GobiModem *modem,
                          GobiModemHandler *handler,
                          const std::string& carrier_name,
                          DBus::Error& error);
  DISALLOW_COPY_AND_ASSIGN(Gobi3KModemHelper);
};

#endif  // PLUGIN_GOBI_3K_MODEM_HELPER_H_
