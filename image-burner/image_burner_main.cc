// Copyright 2010 Google Inc. All Rights Reserved.
// Author: tbarzic@google.com (Toni Barzic)

#include "image_burner/interface.h"
int main(int argc, char* argv[]) {
  g_type_init();

  imageburn::ImageBurnService service_;

  service_.Initialize();
  service_.Register(chromeos::dbus::GetSystemBusConnection());
  service_.Run();
  return 0;
}
