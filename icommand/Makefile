


# =-=-=-=-=-=-=-
# build execs and options
GCC = g++
LDR = g++

# =-=-=-=-=-=-=-
# Directories
objDir = obj
binDir = bin
srcDir = src

#
# Source files
#
OBJS =  $(objDir)/igridreport.o

BINS = $(binDir)/igridreport

# =-=-=-=-=-=-=-
# libraries
BOOST_DIR=/usr/lib/irods/
BOOST_LIBS = \
             $(BOOST_DIR)/libboost_filesystem.a \
             $(BOOST_DIR)/libboost_system.a \
             $(BOOST_DIR)/libboost_regex.a \
             $(BOOST_DIR)/libboost_thread.a

LIBRARY = ../api/libirods_gridreport_client.a \
          -lirods_client \
		  -lirods_client_api \
          $(BOOST_LIBS)

LDFLAGS +=	$(LIBRARY) $(MODULE_LDFLAGS) $(CL_LDADD) 
LDFLAGS += -L/usr/lib/irods/ -rdynamic -lrt -lpthread -ldl -lcurl -lz -lssl -lcrypto 

CFLAGS += -Wno-write-strings -DADDR_64BITS -Dlinux_platform -DNETCDF_API
CFLAGS += -I/usr/include/irods \
          -I../api/include/ \
					$(CFLAGS_OPTIONS) \
					$(LIB_INCLUDES) \
					$(SVR_INCLUDES) \
					$(MODULE_CFLAGS)

CFLAGS_OPTIONS := -g $(CFLAGS) $(MY_CFLAG)

# =-=-=-=-=-=-=-
# Principal Targets
.PHONY: all icommands clients clean 
all:	icommands
	@true

clients: icommands
	@true

icommands: $(OBJS) $(BINS) 
	@true

$(objDir)/%.o:	$(srcDir)/%.cpp 
	@echo "Compile icommand `basename $@`..."
	@$(GCC) -c $(CFLAGS) -o $@ $<

$(binDir)/%:	$(objDir)/%.o 
	@echo "Link icommand `basename $@`..."
	@$(LDR) -o $@ $< $(LDFLAGS)

# Show compile flags
print_cflags:
	@echo "Compile flags:"
	@echo "    $(CFLAGS_OPTIONS)"

# Show link flags
print_ldflags:
	@echo "Link flags:"
	@echo "    $(LDFLAGS)"

# Clean
clean:
	@echo "Cleaning icommands..."
	@rm -f $(BINS) $(OBJS)

