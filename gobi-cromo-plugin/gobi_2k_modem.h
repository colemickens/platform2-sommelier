// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOBI_CROMO_PLUGIN_GOBI_2K_MODEM_H_
#define GOBI_CROMO_PLUGIN_GOBI_2K_MODEM_H_

#include <string>

#include "gobi-cromo-plugin/gobi_modem.h"

class Gobi2KModemHelper : public GobiModemHelper {
 public:
  explicit Gobi2KModemHelper(gobi::Sdk* sdk);
  ~Gobi2KModemHelper();

  virtual void SetCarrier(GobiModem* modem,
                          GobiModemHandler* handler,
                          const std::string& carrier_name,
                          DBus::Error& error);

 private:
  DISALLOW_COPY_AND_ASSIGN(Gobi2KModemHelper);
};

#endif  // GOBI_CROMO_PLUGIN_GOBI_2K_MODEM_H_
