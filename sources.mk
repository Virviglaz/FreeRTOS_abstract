INC			+=	-IFreeRTOS_abstract
SRC			+=	FreeRTOS_abstract/idle_task.o
LDFLAGS		+= -Wl,--wrap,malloc,--wrap,free

#add this to your Makefile
#.ONESHELL:
#include $(shell $(BASH) "$(FIND) -name sources.mk")