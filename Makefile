COMMON_OBJS = CWLog.o tlv.o network.o capwap_message.o capwap_common.o
AC_OBJS = AC.o ac_mainloop.o ac_discovery.o ac_join.o ac_configure.o ac_datacheck.o ac_run.o ac_interface.o uci_config.o ac_manager.o

AC_NAME = AC

CFLAGS += -Wall
LDFLAGS += -levent_core -lpthread -D_REENTRANT -luci -ljson-c

all: $(AC_NAME)

$(AC_NAME): $(AC_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(AC_OBJS) $(COMMON_OBJS) -o $(AC_NAME)
