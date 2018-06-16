


all: client


MODULES += os/services/shell os/net/app-layer/mqtt 

CFLAGS += -Wno-nonnull-compare -Wno-implicit-function-declaration -DTARGET=$(TARGET)
BUILD_WITH_SHELL = 1

PROJECT_SOURCEFILES = movement.c

CONTIKI = ../contiki-ng-course
include $(CONTIKI)/Makefile.include
