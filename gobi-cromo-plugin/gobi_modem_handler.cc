// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_modem_handler.h"

#include <glib.h>
extern "C" {
#include <libudev.h>
#include <stdio.h>
#include <syslog.h>
// Defines from syslog.h that conflict with base/logging.h. Ugh.
#undef LOG_INFO
#undef LOG_WARNING
}

#include <base/logging.h>
#include <cromo/cromo_server.h>
#include <cromo/plugin.h>

#include "device_watcher.h"
#include "gobi_modem.h"
#include "gobi_cdma_modem.h"
#include "gobi_modem_factory.h"
#include "gobi_sdk_wrapper.h"

#ifndef VCSID
#define VCSID "<not set>"
#endif

using std::vector;

static ModemHandler* mm;

static const int kDevicePollIntervalSecs = 1;
static const char* kQCDeviceName = "QCQMI";
static const char* kUSBDeviceListFile = "/var/run/cromo/usb-devices";

static void udev_callback(void* data, const char *action, const char *device) {
  GobiModemHandler* handler = static_cast<GobiModemHandler*>(data);
  handler->HandleUdevMessage(action, device);
}

static void timeout_callback(void* data) {
  GobiModemHandler* handler = static_cast<GobiModemHandler*>(data);
  handler->HandlePollEvent();
}

GobiModemHandler::GobiModemHandler(CromoServer& server)
    : ModemHandler(server, "Gobi"),
      clear_device_list_on_destroy_(true),
      device_watcher_(NULL),
      scan_generation_(0) {
}

GobiModemHandler::~GobiModemHandler() {
  if (clear_device_list_on_destroy_) {
    ClearDeviceListFile();
  }
  delete device_watcher_;
}

bool GobiModemHandler::Initialize() {
  // Can't use LOG here: we want this to be always logged, but we don't want it
  // to be an error. Fortunately syslog declares both openlog() and closelog()
  // 'optional', so...
  syslog(LOG_NOTICE, "gobi-cromo-plugin vcsid %s", VCSID);
  sdk_.reset(new gobi::Sdk(GobiModem::SinkSdkError));
  sdk_->Init();
  GobiModem::set_handler(this);
  MonitorDevices();
  RegisterSelf();
  return true;
}

// Watch for addition and removal of Gobi devices.
// When a udev event arrives, we begin polling
// the SDK until the change reported by the
// event is visible via EnumerateDevices. At that
// point, we stop polling.
void GobiModemHandler::MonitorDevices() {
  device_watcher_ = new DeviceWatcher(kQCDeviceName);
  device_watcher_->set_callback(udev_callback, this);
  device_watcher_->StartMonitoring();
  GetDeviceList();
}

void GobiModemHandler::HandleUdevMessage(const char *action,
                                         const char *device) {
  // udev deals in long device names (like "/dev/qcqmi0") but the qualcomm sdk
  // deals in just basenames (like "qcqmi0"). The control paths we store in the
  // control-path-to-device map are basenames, so only use the device's basename
  // here.
  if (strstr(device, "/dev/") == device)
    device += strlen("/dev/");

  // If this method is called due to an udev event after the poller is started
  // but before the polling callback is invoked, the GetDeviceList() call below
  // may potentially "absorb" any changes in the device list, which means the
  // GetDeviceList() call in the polling callback will return false and keep the
  // polling continuously running. Thus, we stop any scheduled polling here to
  // prevent that from happening.
  device_watcher_->StopPolling();

  bool saw_changes = GetDeviceList();
  if (0 == strcmp("change", action)) {
    // No need to start poller as there is no device added or removed.
    // Otherwise, if we start the poller, it won't be stopped as
    // GetDeviceList() will return false.
    return;
  }

  if (0 == strcmp("add" , action) && DevicePresentByControlPath(device)) {
    LOG(INFO) << "Device already present";
    return;    // Do not start poller
  }

  if (0 == strcmp("remove", action)) {
    // No need to start poller; we have acted on event
    RemoveDeviceByControlPath(device);
    return;
  }

  if (!saw_changes) {
    // The udev change isn't yet visible to QCWWAN.  Poll until it is.
    device_watcher_->StartPolling(kDevicePollIntervalSecs,
                                  timeout_callback,
                                  this);
  } else {
    LOG(ERROR) << "Saw unexpected change " << action << " " << device;
  }
}

void GobiModemHandler::RemoveDeviceByControlPath(const char *path) {
  ControlPathToModem::iterator p = control_path_to_modem_.find(path);
  if (p != control_path_to_modem_.end()) {
    LOG(INFO) << "Removing device " << path;
    RemoveDeviceByIterator(p);
  } else {
    LOG(INFO) << "Could not find " << path << " to remove";
  }
}

static gboolean delete_modem(gpointer p) {
  GobiModem *m = static_cast<GobiModem*>(p);
  delete m;
  return FALSE;
}

GobiModemHandler::ControlPathToModem::iterator
GobiModemHandler::RemoveDeviceByIterator(ControlPathToModem::iterator p) {
  if (p == control_path_to_modem_.end()) {
    LOG(ERROR) << "Bad iterator";
    return p;
  }
  ControlPathToModem::iterator next = p;
  ++next;

  GobiModem *m = p->second;
  if (m == NULL) {
    LOG(ERROR) << "no modem";
    return next;
  }
  server().DeviceRemoved(m->path());
  control_path_to_modem_.erase(p);
  g_idle_add(delete_modem, m);
  return next;
}

void GobiModemHandler::Remove(GobiModem* modem) {
  for (ControlPathToModem::iterator p = key_to_modem_.begin();
       p != key_to_modem_.end(); ++p) {
    GobiModem* m = p->second;
    if (m == modem) {
      RemoveDeviceByIterator(p);
    }
  }
}


bool GobiModemHandler::DevicePresentByControlPath(const char *path) {
  return control_path_to_modem_.find(path) != control_path_to_modem_.end();
}

void GobiModemHandler::HandlePollEvent() {
  if (GetDeviceList())
    device_watcher_->StopPolling();
}

// Get the list of visible devices, keeping track of what devices have
// been added and removed since the last time we looked. Returns true
// if any devices have been added or removed, false otherwise.
bool GobiModemHandler::GetDeviceList() {
  const int MAX_MODEMS = 16;
  ULONG rc;

  gobi::DeviceElement devices[MAX_MODEMS];
  BYTE num_devices = MAX_MODEMS;

  rc = sdk_->QCWWANEnumerateDevices(&num_devices,
                                    reinterpret_cast<BYTE*>(devices));
  if (rc != 0) {
    LOG(ERROR) << "QCWWANEnumerateDevices returned " << rc;
    return false;
  }

  ++scan_generation_;
  bool something_changed = false;

  LOG(INFO) << "QCWWANEnumerateDevices found " << static_cast<int>(num_devices)
            << " device(s)";
  for (size_t i = 0; i < num_devices; ++i) {
    ControlPathToModem::iterator p =
        control_path_to_modem_.find(devices[i].deviceNode);
    if (p != control_path_to_modem_.end()) {
      CHECK((*p).second);
      (*p).second->set_last_seen(scan_generation_);
    } else {
      something_changed = true;
      GobiModem* m = GobiModemFactory::CreateModem(server().conn(),
                                                   MakePath(),
                                                   devices[i],
                                                   sdk_.get());
      if (m == NULL) {
        LOG(ERROR) << "Could not create modem object for "
                   << devices[i].deviceNode;
        continue;
      }
      m->Init();
      m->set_last_seen(scan_generation_);
      control_path_to_modem_[std::string(devices[i].deviceNode)] = m;
      server().DeviceAdded(m->path());
      LOG(INFO) << "Found new modem: " << m->path()
                << " (" << devices[i].deviceNode << ")";
    }
  }

  for (ControlPathToModem::iterator p = control_path_to_modem_.begin();
       p != control_path_to_modem_.end(); ) {
    GobiModem* m = p->second;
    if (m->last_seen() != scan_generation_) {
      something_changed = true;
      LOG(INFO) << "Device " << m->path() << " has disappeared";
      p = RemoveDeviceByIterator(p);
    } else {
      ++p;
    }
  }

  WriteDeviceListFile(control_path_to_modem_);

  return something_changed;
}

void GobiModemHandler::ClearDeviceListFile() {
  ControlPathToModem no_modems;
  WriteDeviceListFile(no_modems);
}

void GobiModemHandler::WriteDeviceListFile(const ControlPathToModem &modems) {
  FILE *dev_list_file = fopen(kUSBDeviceListFile, "w");
  if (dev_list_file) {
    for (ControlPathToModem::const_iterator p = modems.begin();
         p != modems.end(); p++) {
      GobiModem *m = p->second;
      fprintf(dev_list_file, "%s\n", m->GetUSBAddress().c_str());
    }
    fclose(dev_list_file);
  }
}

// Enumerate the existing devices, and add them to the list of devices
// that are managed by the ChromeOS modem manager

vector<DBus::Path> GobiModemHandler::EnumerateDevices(DBus::Error& error) {
  vector <DBus::Path> to_return;
  for (ControlPathToModem::iterator p = control_path_to_modem_.begin();
       p != control_path_to_modem_.end(); ++p) {
    to_return.push_back(p->second->path());
  }
  return to_return;
}

GobiModem* GobiModemHandler::LookupByDbusPath(const std::string& dbuspath)
{
  for (ControlPathToModem::iterator p = control_path_to_modem_.begin();
       p != control_path_to_modem_.end(); ++p) {
    GobiModem* m = p->second;
    if (m->path() == dbuspath)
      return m;
  }
  return NULL;
}

void GobiModemHandler::ExitLeavingModemsForCleanup() {
  clear_device_list_on_destroy_ = false;
  LOG(ERROR) << "Exiting without clearing device list.";
  exit(1);
}

static void onload(CromoServer* server) {
  mm = new GobiModemHandler(*server);
  if (!mm->Initialize())
    LOG(ERROR) << "Failed to initialize GobiModemHandler";
}

static void onunload() {
  delete mm;
}

CROMO_DEFINE_PLUGIN(gobi, onload, onunload)
