#include "image_burner/bindings/client.h"
#include "image_burner/marshal.h"
#include <glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <base/basictypes.h>
#include <chromeos/dbus/dbus.h>
#include <iostream>

namespace chromeos {

class TestClient{
 public:
  typedef dbus::MonitorConnection<void (const char*, int64, int64)>*
                                                        ConnectionUpdateType;
  typedef dbus::MonitorConnection<void (const char*, bool, const char*)>*
                                                        ConnectionFinishedType;
  static void Update(void* object, const char* target_path,
                        int64 amount_burnt, int64 total_size) {

      std::cout<<target_path<<" "<<amount_burnt<<" "<<total_size<<std::endl;
      flush(std::cout);
       
    }

    static void Finished(void* object, const char* target_path, bool success,
                          const char* error ) {
      std::cout<<target_path<<" "<<success<<" "<<error<<std::endl;
    }
  TestClient(dbus::Proxy proxy) : burn_proxy_(proxy){


    ::dbus_g_proxy_add_signal(burn_proxy_.gproxy(),
                              "burn_progress_update",
                                G_TYPE_STRING,
                                G_TYPE_INT64,
                                G_TYPE_INT64,
                                G_TYPE_INVALID);

 typedef dbus::MonitorConnection<void (const char*, int64, int64)> ConnectionUpdateTypeRef;

    ConnectionUpdateTypeRef *update = new ConnectionUpdateTypeRef(burn_proxy_,
                                                                  "burn_progress_update", &TestClient::Update, this);

      ::dbus_g_proxy_connect_signal(burn_proxy_.gproxy(),
                                    "burn_progress_update",
                                    G_CALLBACK(&ConnectionUpdateTypeRef::Run),
                                    update, NULL);


      // adding finished signal and connection
      ::dbus_g_proxy_add_signal(burn_proxy_.gproxy(),
                                "burn_finished",
                                G_TYPE_STRING,
                                G_TYPE_BOOLEAN,
                                G_TYPE_STRING,
                                G_TYPE_INVALID);
updateconnection_=update;

typedef dbus::MonitorConnection<void (const char*, bool, const char*)> ConnectionFinishTypeRef;


ConnectionFinishTypeRef *finished = new ConnectionFinishTypeRef(burn_proxy_,
                                                                "burn_finished",
          &TestClient::Finished, this);
      ::dbus_g_proxy_connect_signal(burn_proxy_.gproxy(),
                                    "burn_finished",
                                    G_CALLBACK(&ConnectionFinishTypeRef::Run),
                                    finished, NULL);
finishedconnection_=finished;

  }


  ~TestClient(){
    dbus::Disconnect(updateconnection_);
    dbus::Disconnect(finishedconnection_);
  }


  void Run(const char* from, const char* to){
    glib::ScopedError error;

    std::cout<<"start"<<std::endl;
    ::dbus_g_proxy_call(burn_proxy_.gproxy(),
                        "BurnImage",
                                &Resetter(&error).lvalue(),
                                G_TYPE_STRING, from,
                                G_TYPE_STRING, to,
                                G_TYPE_INVALID);
    std::cout<<"end+++++++++++++++++++++++++++"<<std::endl;

  }
private:
  dbus::Proxy burn_proxy_;
  ConnectionUpdateType updateconnection_;
  ConnectionFinishedType finishedconnection_;
};

}

int main(int argc, char* argv[]) {
  ::g_type_init();
   GMainLoop* loop = ::g_main_loop_new(NULL, false);
   std::cout<<"A";
   dbus_g_object_register_marshaller(image_burner_VOID__STRING_BOOLEAN_STRING,
                                    G_TYPE_NONE,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN,
                                    G_TYPE_STRING,
                                    G_TYPE_INVALID);
  dbus_g_object_register_marshaller(image_burner_VOID__STRING_INT64_INT64,
                                    G_TYPE_NONE,
                                    G_TYPE_STRING,
                                    G_TYPE_INT64,
                                    G_TYPE_INT64,
                                    G_TYPE_INVALID);

 chromeos::dbus::BusConnection bus = chromeos::dbus::GetSystemBusConnection();
    chromeos::dbus::Proxy burn_proxy(bus,
                           "org.chromium.ImageBurner",
                           "/org/chromium/ImageBurner",
                           "org.chromium.ImageBurnerInterface");

   chromeos::TestClient test(burn_proxy);

  test.Run("/home/chronos/user/Downloads/ChromeOS-0.7.42.11-raa4a3ca2.bin.gz","/home/chronos/user/Downloads/image_test.bin");
  test.Run("/home/chronos/user/Downloads/ChromeOS-0.7.42.11-raa4a3ca2.bin.gz","/dev/sdb");
  while(true);


return 0;
}


