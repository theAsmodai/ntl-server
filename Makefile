OPT_FLAGS = -O3 -funroll-loops -s -pipe -fomit-frame-pointer -fno-strict-aliasing -fvisibility=hidden -mfpmath=sse -march=core2 -std=c11
DEBUG_FLAGS = -g -ggdb3
COMPILER = gcc-4.9
NAME = ntl-server

OBJECTS = client.c config.c database.c main.c mem.c net.c servers.c sys.c util.c hash/md5.c hash/sha1.c hash/sha256.c

INCLUDE = -I. -I./hash -I/usr/include/mysql

LINK = -lpthread -pthread -lmysqlclient -lm

GCC_VERSION := $(shell $(COMPILER) -dumpversion >&1 | cut -b1)

ifeq "$(DEBUG)" "true"
	BIN_DIR = Debug
	CFLAGS = $(DEBUG_FLAGS)
else
	BIN_DIR = Release
	CFLAGS = $(OPT_FLAGS)
endif

CFLAGS += -DNDEBUG -Wno-write-strings -Wno-deprecated -Wmultichar -DHAVE_STDINT_H -static-libgcc -m32 -DREVISION=$(REVISION)

BINARY = $(NAME)
OBJ_LINUX := $(OBJECTS:%.c=$(BIN_DIR)/%.o)

$(BIN_DIR)/%.o: %.c
	$(COMPILER) $(INCLUDE) $(CFLAGS) -o $@ -c $<

all:
	mkdir -p $(BIN_DIR)
	mkdir -p $(BIN_DIR)/hash

	$(MAKE) $(NAME)

ntl-server: $(OBJ_LINUX)
	$(COMPILER) $(INCLUDE) $(CFLAGS) $(OBJ_LINUX) $(LINK) -o$(BIN_DIR)/$(BINARY)

check:
	cppcheck $(INCLUDE) --quiet --max-configs=100 -D__linux__ -D_GNU_SOURCE -DNDEBUG -DHAVE_STDINT_H .

debug:	
	$(MAKE) all DEBUG=false

default: all

clean:
	rm -rf Release/hash/*.o
	rm -rf Release/*.o
	rm -rf Release/$(NAME)
	rm -rf Release/$(NAME)
	rm -rf Debug/hash/*.o
	rm -rf Debug/*.o
	rm -rf Debug/$(NAME)
	rm -rf Debug/$(NAME)
