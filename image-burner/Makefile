
XXFLAGS ?= -Wall -Werror -g
PKG_CONFIG ?= pkg-config

BASE_LIBS = -lbase -lpthread -lrt -lchromeos -lz 
LIBS = $(BASE_LIBS) -lcrash
TEST_LIBS = $(BASE_LIBS)
INCLUDE_DIRS = -I.. \
	$(shell $(PKG_CONFIG) --cflags gobject-2.0 dbus-1 dbus-glib-1)
LIB_DIRS = $(shell $(PKG_CONFIG) --libs gobject-2.0 dbus-1 dbus-glib-1 )

IMAGEBURN_COMMON_OBJS = service.o  interface.o

DBUS_SOURCE = image_burner.xml
DBUS_SOURCE_BASENAME = image_burner
DBUS_SERVER = bindings/server.h
DBUS_CLIENT = bindings/client.h

DBUS_MARSHAL_HEADER = marshal.h
DBUS_MARSHAL_SOURCE = marshal.cc
DBUS_MARSHAL_LIST = marshal.list



IMAGEBURN_BIN = image_burner
IMAGEBURN_OBJS = $(IMAGEBURN_COMMON_OBJS) image_burner_main.o

TESTER_BIN = image_burner_tester
TESTER_OBJS = image_burner_tester.o

BINDINGS_DIR = bindings


all: $(IMAGEBURN_BIN)  $(TESTER_BIN)

$(IMAGEBURN_OBJS) : $(DBUS_SERVER) $(DBUS_MARSHAL_SOURCE) 

$(TESTER_OBJS) : $(DBUS_CLIENT) $(DBUS_MARSHAL_SOURCE) 

$(IMAGEBURN_BIN): $(IMAGEBURN_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(IMAGEBURN_OBJS) \
	$(LIBS) $(LDFLAGS) -o $(IMAGEBURN_BIN)

$(TESTER_BIN): $(TESTER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS) $(TESTER_OBJS) \
	$(TEST_LIBS) $(LDFLAGS) -o $(TESTER_BIN)


.cc.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

$(BINDINGS_DIR):
	mkdir -p $(BINDINGS_DIR)

$(DBUS_SERVER): $(DBUS_SOURCE) $(BINDINGS_DIR)
	dbus-binding-tool --mode=glib-server \
	 --prefix=$(DBUS_SOURCE_BASENAME) $(DBUS_SOURCE) > $(DBUS_SERVER)

$(DBUS_CLIENT): $(DBUS_SOURCE) $(BINDINGS_DIR)
	dbus-binding-tool --mode=glib-client \
	 --prefix=$(DBUS_SOURCE_BASENAME) $(DBUS_SOURCE) > $(DBUS_CLIENT)

$(DBUS_MARSHAL_HEADER) : $(DBUS_MARSHAL_LIST)  
   glib-genmarshal --header --prefix=image_burner' \
          $(DBUS_MARSHAL_LIST) > $(DBUS_MARSHAL_HEADER)
 
$(DBUS_MARSHAL_SOURCE) :  $(DBUS_MARSHAL_HEADER)
   glib-genmarshal --body --prefix=image_burner' \
          $(DBUS_MARSHAL_LIST) > $(DBUS_MARSHAL_SOURCE)

clean:
	rm -rf  *.o $(BINDINGS_DIR) $(DBUS_MARSHAL_HEADER)  $(DBUS_MARSHAL_SOURCE)


