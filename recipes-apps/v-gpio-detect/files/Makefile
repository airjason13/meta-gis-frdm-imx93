MAKE_DIR = $(PWD)

APP = app
UTILDBGLIB = utildbglib

APP_DIR := $(MAKE_DIR)/$(APP)
UTILDBGLIB_DIR := $(MAKE_DIR)/$(UTILDBGLIB)

INC_SRCH_PATH := -I$(MAKE_DIR)
INC_SRCH_PATH += -I$(MAKE_DIR)/recipe-sysroot/usr/include/SDL2/
INC_SRCH_PATH += -I$(MAKE_DIR)/recipe-sysroot/usr/include/libusb-1.0/
INC_SRCH_PATH += -I$(UTILDBGLIB_DIR)

LIB_SRCH_PATH := -L$(MAKE_DIR)
LIB_SRCH_PATH += -L$(MAKE_DIR)/libs/

CFLAGS :=
CFLAGS += $(INC_SRCH_PATH) $(LIB_SRCH_PATH)
CFLAGS += -DDEBUG -D_REENTRANT
CFLAGS += -Wall 
CFLAGS += -D_GNU_SOURCE 

LDFLAGS :=
LDFLAGS += -lutildbg
LDFLAGS += -lpthread
LDFLAGS += -lgpiod

export MAKE_DIR CFLAGS LDFLAGS LIBS INC_SRCH_PATH LIB_SRCH_PATH

all: 
	echo $(CFLAGS)
	$(MAKE) -C $(UTILDBGLIB_DIR) -f $(UTILDBGLIB).mk
	$(MAKE) -C $(APP_DIR) -f $(APP).mk
	
.PHONY: clean
clean:
	$(MAKE) -C $(UTILDBGLIB_DIR) -f $(UTILDBGLIB).mk clean
	$(MAKE) -C $(APP_DIR) -f $(APP).mk clean
