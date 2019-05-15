TESTS = vfio_mlx5

all: ${TESTS}

CFLAGS += -Wall -g -D_GNU_SOURCE -O2
BASIC_FILES =
EXTRA_FILES =
BASIC_HEADERS =
EXTRA_HEADERS =
#The following seems to help GNU make on some platforms
LOADLIBES +=
LDFLAGS +=

${TESTS}: LOADLIBES +=

${TESTS} : %: %.c ${BASIC_FILES} ${EXTRA_FILES} ${BASIC_HEADERS} ${EXTRA_HEADERS}
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< ${BASIC_FILES} ${EXTRA_FILES} $(LOADLIBES) $(LDLIBS) -o $@

clean:
	$(foreach fname,${TESTS} , rm -f ${fname})
.DELETE_ON_ERROR:
.PHONY: all clean
