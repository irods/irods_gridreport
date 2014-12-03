TARGET = libirods_zonereport.a

SRCS =  gather_resources.cpp \
        zone_server_properties.cpp \
        rcZoneReport.cpp \
        rcServerReport.cpp 

HEADERS =

EXTRALIBS = /usr/lib/libirods.a

EXTRAINCS = -I/usr/include/irods/jansson/src/

######################################################################
# General settings below.
# These should not need to be changed in most cases.

SODIR = ..
OBJDIR = .objs
SVR_OBJDIR = .svr_objs
CLI_OBJDIR = .cli_objs

FULLTARGET = ${SODIR}/${TARGET}
SVR_TARGET= ${SODIR}/libirods_zonereport_server.a
CLI_TARGET= ${SODIR}/libirods_zonereport_client.a

OBJS = $(patsubst %.cpp, ${OBJDIR}/%.o, ${SRCS})
SVR_OBJS = $(patsubst %.cpp, ${SVR_OBJDIR}/%.o, ${SRCS})
CLI_OBJS = $(patsubst %.cpp, ${CLI_OBJDIR}/%.o, ${SRCS})

GCC = g++

CFLAGS = -Wno-write-strings

INC = -I../include/ -I/usr/include/irods ${EXTRAINCS}

.PHONY: default clean client-library server-library

default: ${FULLTARGET}

client-library: ${CLI_TARGET}

server-library: ${SVR_TARGET}

clean:
	@-rm -f ${FULLTARGET} > /dev/null 2>&1
	@-rm -f ${SVR_TARGET} > /dev/null 2>&1
	@-rm -f ${CLI_TARGET} > /dev/null 2>&1
	@-rm -f ${OBJS} > /dev/null 2>&1
	@-rm -f ${CLI_OBJS} > /dev/null 2>&1
	@-rm -f ${SVR_OBJS} > /dev/null 2>&1

${FULLTARGET}: ${OBJS} 
	ar  -r -c -s ${FULLTARGET} ./${OBJDIR}/*.o

${SVR_TARGET}: ${SVR_OBJS} 
	ar  -r -c -s ${SVR_TARGET} ./${SVR_OBJDIR}/*.o

${CLI_TARGET}: ${CLI_OBJS} 
	ar  -r -c -s ${CLI_TARGET} ./${CLI_OBJDIR}/*.o

$(OBJS): ${OBJDIR}/%.o: %.cpp
	@-mkdir -p ${OBJDIR} > /dev/null 2>&1
	${GCC} ${CFLAGS} ${INC} -fPIC -c -g -o $@ $<

$(SVR_OBJS): ${SVR_OBJDIR}/%.o: %.cpp
	@-mkdir -p ${SVR_OBJDIR} > /dev/null 2>&1
	${GCC} ${CFLAGS} -DRODS_SERVER ${INC} -fPIC -c -g -o $@ $<

$(CLI_OBJS): ${CLI_OBJDIR}/%.o: %.cpp
	@-mkdir -p ${CLI_OBJDIR} > /dev/null 2>&1
	${GCC} ${CFLAGS} ${INC} -fPIC -c -g -o $@ $<

