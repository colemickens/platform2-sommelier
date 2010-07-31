// Copyright 2010 Google Inc. All Rights Reserved.
// Author: tbarzic@google.com (Toni Barzic)


#ifndef IMAGEBURN_SERVICE_H_
#define IMAGEBURN_SERVICE_H_


#include <base/basictypes.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>



namespace imageburn {

namespace gobject {
struct ImageBurner;
}  // namespace gobject

const int kNumSignals = 2;

enum BurnSignals{
  kSignalBurnFinished,
  kSignalBurnUpdate
};




// Provides a wrapper for exporting SessionManagerInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
class ImageBurnService : public chromeos::dbus::AbstractDbusService {

 public:
  ImageBurnService();
  virtual ~ImageBurnService();

  virtual bool Initialize();
  virtual bool Reset();
  virtual bool Shutdown();  

  virtual const char *service_name() const;

  virtual const char *service_path() const;

  virtual const char *service_interface() const;

  virtual GObject *service_object() const;

  gboolean BurnImage(gchar* from_path, gchar* to_path, GError** error);

  gboolean UnmountDevice(gchar* device_path);

 protected:
  virtual GMainLoop *main_loop() {
    return main_loop_;
  }

 private:
  bool DoBurn(gchar* from_path, gchar* to_path);
  void SendProgressSignal(int64 amount_burnt, int64 total_size,
                          const char* target_path);
  void SendFinishedSignal(const char* target_path, bool success, const char* error);

  //reads last 4 bytes from gzip file (which hold uncompressed content size)
  //maximum content size supported is 4GB
  int64 GetTotalImageSize(gchar* from_path);

  gobject::ImageBurner* image_burner_;
  GMainLoop* main_loop_;
  guint signals_[kNumSignals];
  bool shutting_down_;
};

} // namespace imageburn
#endif /* IMAGEBURN_SERVICE_H_ */
