all: client


MODULES += os/net/app-layer/mqtt 

#Use tsch to have a better radio duty cycling
#MAKE_MAC = MAKE_MAC_TSCH

CFLAGS += -Os -Wno-nonnull-compare -Wno-implicit-function-declaration -DTARGET=$(TARGET)

PROJECT_SOURCEFILES = movement.c energest-log.c led-report.c

CONTIKI = ../contiki-ng-course
include $(CONTIKI)/Makefile.include
