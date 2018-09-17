all: client


MODULES += os/services/shell os/net/app-layer/mqtt 

#Uses tsch to have a better radio duty cycling
#MAKE_MAC = MAKE_MAC_TSCH

CFLAGS += -Wno-nonnull-compare -Wno-implicit-function-declaration -DTARGET=$(TARGET)
#BUILD_WITH_SHELL = 1

PROJECT_SOURCEFILES = movement.c

CONTIKI = ../contiki-ng-course
include $(CONTIKI)/Makefile.include
