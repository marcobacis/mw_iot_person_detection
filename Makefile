

all: client

MODULES += os/services/shell
CFLAGS += -Wno-nonnull-compare -Wno-implicit-function-declaration
BUILD_WITH_SHELL = 1

CONTIKI = ../contiki-ng-course
include $(CONTIKI)/Makefile.include
