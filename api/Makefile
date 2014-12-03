SERVER_LIBRARY = libirods_zonereport_server.a
CLIENT_LIBRARY = libirods_zonereport_client.a
SRCDIR = src

SRCS = $(SRCDIR)/rsZoneReport.cpp \
       $(SRCDIR)/rsServerReport.cpp 
 
MAKEFLAGS += --no-print-directory

CFLAGS += -Wno-write-strings
CFLAGS += -L/usr/lib/irods

EXTRAINCS += -I/usr/include/irods/jansson/src/

BOOST_LIBS = \
    -lboost_filesystem \
    -lboost_system \
    -lboost_regex \
    -lboost_thread

EXTRALIBS += $(BOOST_LIBS) \
             -lirods_client \
             -ljansson

SERVER_LIBS += $(SERVER_LIBRARY)
CLIENT_LIBS += $(CLIENT_LIBRARY)

.PHONY: default clean library

default: api-base

library: $(SERVER_LIBRARY) $(CLIENT_LIBRARY)

$(SERVER_LIBRARY):
	@${MAKE} -C lib server-library

$(CLIENT_LIBRARY):
	@${MAKE} -C lib client-library

clean: clean-base
	@${MAKE} -C lib clean

include ../Makefile.base
