cm_default_SOURCES=cm_default/main.c cm_default/QmiWwanCM.c cm_default/GobiNetCM.c  cm_default/MPQMUX.c cm_default/QMIThread.c cm_default/util.c cm_default/udhcpc.c
qmi-proxy_SOURCES=cm_default/qmi-proxy.c

comma = ,
try-run = $(shell set -e; ($(1)) >/dev/null 2>&1 && echo "$(2)" || echo "$(3)")
cc-option = $(call try-run, $(CC) $(1) -c -xc /dev/null -o /dev/null,$(1),$(2))

CFLAGS ?= -s
WFLAGS := -Wall \
	$(call cc-option,-Wextra) \
	$(call cc-option,-Wwrite-strings) \
	$(call cc-option,-Wno-sign-compare) \
	$(call cc-option,-Wno-unused-function)
CFLAGS += $(WFLAGS)

all:oldconnmanager muxconnmanager qmi-proxy
	echo "Successfully to build default dial and doubapn dial program."

oldconnmanager:cleanold
	$(CC) $(CFLAGS) ${cm_default_SOURCES}  -o cm_default/oldconnmanager -Icm_default -lpthread
qmi-proxy:cleanproxy
	$(CC) $(CFLAGS) ${qmi-proxy_SOURCES} -o cm_default/qmi-proxy -Icm_default -lpthread
muxconnmanager: cleanmux
	$(CC) -Wall -s QmiWwanCM.c  main.c MPQMUX.c QMIThread.c util.c  -o muxconnmanager -lpthread

cleanmux:
	rm -rf muxconnmanager ./*.o *~
cleanold:
	rm -rf cm_default/oldconnmanager cm_default/*.o cm_default/*~
cleanproxy:
	rm -rf cm_default/qmi-proxy cm_default/*.o cm_default/*~
.PHONY:clean
clean:
	rm -rf muxconnmanager ./*.o *~ cm_default/oldconnmanager cm_default/*.o cm_default/*~ cm_default/qmi-proxy

