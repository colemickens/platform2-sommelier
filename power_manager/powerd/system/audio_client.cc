// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/audio_client.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/system/audio_observer.h"

namespace power_manager {
namespace system {

namespace {

// Frequency with which observers should be notified while audio is playing, in
// milliseconds.
const int kNotifyObserversMs = 5000;

// Maximum amount of time to wait for a reply after making a D-Bus method call
// to CRAS.
const int kDBusTimeoutMs = 5000;

// Log a warning if a D-Bus call takes longer than this to complete.
const int kDBusSlowCallMs = 1000;

// Keys within node dictionaries returned by CRAS.
const char kTypeKey[] = "Type";
const char kActiveKey[] = "Active";

// Types assigned to headphone and HDMI nodes by CRAS.
const char kHeadphoneNodeType[] = "HEADPHONE";
const char kHdmiNodeType[] = "HDMI";

// Creates and returns a call to CRAS's D-Bus method named |method_name|. The
// caller takes ownership of the returned message.
DBusMessage* CreateRequest(const std::string& method_name) {
  DBusMessage* request = dbus_message_new_method_call(
      cras::kCrasServiceName, cras::kCrasServicePath,
      cras::kCrasControlInterface, method_name.c_str());
  CHECK(request);
  return request;
}

// Invokes and frees |request| and returns the reply. The caller must free the
// reply via dbus_message_unref(). Returns NULL on failure.
DBusMessage* CallMethodAndUnref(DBusMessage* request) {
  const std::string name = dbus_message_get_member(request);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);

  DBusError error;
  dbus_error_init(&error);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  DBusMessage* reply = dbus_connection_send_with_reply_and_block(
      connection, request, kDBusTimeoutMs, &error);
  dbus_message_unref(request);

  const base::TimeDelta duration = base::TimeTicks::Now() - start_time;
  if (duration.InMilliseconds() > kDBusSlowCallMs)
    LOG(WARNING) << name << " call took " << duration.InMilliseconds() << " ms";

  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << name << " call failed: " << error.name
               << " (" << error.message << ")";
    dbus_error_free(&error);
  }

  // dbus_connection_send_with_reply_and_block() is documented as returning NULL
  // in the case of an error.
  return reply;
}

// Sends a request to CRAS asking it to mute or unmute the system volume.
void SetOutputMute(bool mute) {
  DBusMessage* request = CreateRequest(cras::kSetOutputMute);
  dbus_bool_t mute_arg = mute;
  dbus_message_append_args(request, DBUS_TYPE_BOOLEAN, &mute_arg,
                           DBUS_TYPE_INVALID);
  DBusMessage* reply = CallMethodAndUnref(request);
  if (reply)
    dbus_message_unref(reply);
}

}  // namespace

AudioClient::AudioClient()
    : num_active_streams_(0),
      headphone_jack_plugged_(false),
      hdmi_active_(false),
      mute_stored_(false),
      originally_muted_(false),
      notify_observers_timeout_id_(0) {
}

AudioClient::~AudioClient() {
  util::RemoveTimeout(&notify_observers_timeout_id_);
}

void AudioClient::AddObserver(AudioObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AudioClient::RemoveObserver(AudioObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void AudioClient::MuteSystem() {
  if (mute_stored_)
    return;

  DBusMessage* reply = CallMethodAndUnref(CreateRequest(cras::kGetVolumeState));
  if (reply) {
    DBusError error;
    dbus_error_init(&error);
    dbus_int32_t output_volume = 0;
    dbus_bool_t output_mute = FALSE;
    if (dbus_message_get_args(reply, &error,
                              DBUS_TYPE_INT32, &output_volume,
                              DBUS_TYPE_BOOLEAN, &output_mute,
                              DBUS_TYPE_INVALID)) {
      originally_muted_ = output_mute;
      mute_stored_ = true;
    } else {
      LOG(WARNING) << "Unable to read " << cras::kGetVolumeState << " args";
    }
    dbus_message_unref(reply);
  }

  if (mute_stored_) {
    VLOG(1) << "Muting system; old state was " << originally_muted_;
    SetOutputMute(true);
  }
}

void AudioClient::RestoreMutedState() {
  if (!mute_stored_)
    return;

  VLOG(1) << "Restoring system mute state to " << originally_muted_;
  SetOutputMute(originally_muted_);
  mute_stored_ = false;
}

void AudioClient::LoadInitialState() {
  UpdateDevices();
  UpdateNumActiveStreams();
}

void AudioClient::UpdateDevices() {
  headphone_jack_plugged_ = false;
  hdmi_active_ = false;

  DBusMessage* reply = CallMethodAndUnref(CreateRequest(cras::kGetNodes));
  if (!reply)
    return;

  // At the outer level, there's a dictionary (which D-Bus represents as an
  // array) corresponding to each node.
  DBusMessageIter array_iter;
  for (dbus_message_iter_init(reply, &array_iter);
       dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID;
       dbus_message_iter_next(&array_iter)) {
    // Check that the value is an array of dictionary entries mapping from
    // strings to variants.
    const std::string signature = dbus_message_iter_get_signature(&array_iter);
    if (signature != "a{sv}") {
      LOG(WARNING) << "Skipping entry with signature " << signature;
      continue;
    }

    const char* type_str = "";
    dbus_bool_t active = FALSE;

    // Descend into the dictionary to iterate through its entries.
    DBusMessageIter dict_iter;
    for (dbus_message_iter_recurse(&array_iter, &dict_iter);
         dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_INVALID;
         dbus_message_iter_next(&dict_iter)) {
      // Descend into each dictionary entry to read its key and value.
      DBusMessageIter entry_iter;
      dbus_message_iter_recurse(&dict_iter, &entry_iter);
      DCHECK_EQ(dbus_message_iter_get_arg_type(&entry_iter), DBUS_TYPE_STRING);
      const char* key_str = NULL;
      dbus_message_iter_get_basic(&entry_iter, &key_str);
      const std::string key = key_str;

      // The value is stored as a variant, which we need to again descend into
      // to get the actual typed value.
      dbus_message_iter_next(&entry_iter);
      DCHECK_EQ(dbus_message_iter_get_arg_type(&entry_iter), DBUS_TYPE_VARIANT);
      DBusMessageIter value_iter;
      dbus_message_iter_recurse(&entry_iter, &value_iter);
      const int value_type = dbus_message_iter_get_arg_type(&value_iter);

      if (key == kTypeKey && value_type == DBUS_TYPE_STRING)
        dbus_message_iter_get_basic(&value_iter, &type_str);
      else if (key == kActiveKey && value_type == DBUS_TYPE_BOOLEAN)
        dbus_message_iter_get_basic(&value_iter, &active);
    }

    const std::string type = type_str;
    VLOG(1) << "Saw node: type=" << type << " active=" << active;

    // The D-Bus interface doesn't return unplugged nodes.
    if (type == kHeadphoneNodeType)
      headphone_jack_plugged_ = true;
    else if (type == kHdmiNodeType && active)
      hdmi_active_ = true;
  }

  dbus_message_unref(reply);

  LOG(INFO) << "Updated audio devices: headphones "
            << (headphone_jack_plugged_ ? "" : "un") << "plugged, "
            << "HDMI " << (hdmi_active_ ? "" : "in") << "active";
}

void AudioClient::UpdateNumActiveStreams() {
  dbus_int32_t num_streams = 0;
  DBusMessage* reply = CallMethodAndUnref(
      CreateRequest(cras::kGetNumberOfActiveStreams));
  if (reply) {
    DBusError error;
    dbus_error_init(&error);
    if (!dbus_message_get_args(reply, &error,
                               DBUS_TYPE_INT32, &num_streams,
                               DBUS_TYPE_INVALID)) {
      LOG(WARNING) << "Unable to read " << cras::kGetVolumeState << " args";
    }
    dbus_message_unref(reply);
  }

  const int old_num_streams = num_active_streams_;
  num_active_streams_ = std::max(num_streams, 0);

  if (num_active_streams_ && !old_num_streams) {
    LOG(INFO) << "Audio playback started";
    NotifyObservers();
    DCHECK_EQ(notify_observers_timeout_id_, 0U);
    notify_observers_timeout_id_ = g_timeout_add(
        kNotifyObserversMs, NotifyObserversThunk, this);
  } else if (!num_active_streams_ && old_num_streams) {
    LOG(INFO) << "Audio playback stopped";
    util::RemoveTimeout(&notify_observers_timeout_id_);
  }
}

gboolean AudioClient::NotifyObservers() {
  // TODO(derat): Instead of notifying repeatedly, only notify when playback
  // starts or stops. This will require reworking StateController.
  DCHECK_GT(num_active_streams_, 0);
  FOR_EACH_OBSERVER(AudioObserver, observers_,
                    OnAudioActivity(base::TimeTicks::Now()));
  return TRUE;
}

}  // namespace system
}  // namespace power_manager
