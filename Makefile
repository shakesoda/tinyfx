OUTPUT  =tinyfx
# shut up stb.h warnings that show up when we are using -Wall...
SHUTUP  =-Wno-pointer-to-int-cast -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value
CFLAGS  =-fPIC -Wall -Wno-deprecated-declarations -ftree-vectorize -pipe -Wno-psabi $(SHUTUP) -I. -ggdb
LDFLAGS = -lpthread -lrt -ldl -lm
DEMO = examples/demo.c
SOURCES = tinyfx.c $(DEMO)
OBJECTS = $(SOURCES:.c=.o)

# yes, make, use my damn cores.
CORES   = $(shell getconf _NPROCESSORS_ONLN)
MAKEFLAGS := -j $(CORES)

ifneq ("$(wildcard /opt/vc/include/bcm_host.h)", "")
	RPI := 1
endif

ifeq ($(DEBUG), 1)
	CFLAGS += -g -DDEBUG
endif

ifeq ($(RPI), 1)
	CFLAGS  += -DRPI=1 -I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
	LDFLAGS += -L/opt/vc/lib -lEGL -lbcm_host -lvcos -lvchiq_arm -lSDL2
else
	LDFLAGS += -lSDL2
endif

all: $(OBJECTS)
	$(CC) $(OBJECTS) -o $(OUTPUT) -Wl,--whole-archive $(LDFLAGS) -Wl,--no-whole-archive -rdynamic

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

run: all
	./$(OUTPUT)

rebuild: clean all

clean:
	rm -f $(OUTPUT) $(OBJECTS)

release: all
	strip -p $(OUTPUT)

.PHONY: clean all release
.NOTPARALLEL: clean
