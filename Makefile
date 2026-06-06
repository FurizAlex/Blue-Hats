CC = gcc
CFLAGS = -Wall -I. -Isrc
LIBS = -lcurl -larchive -lssl -lcrypto

SRCS = \
	src/main.c		\
	src/error.c		\
	src/add.c		\
	src/publish.c	\
	src/install.c	\
	src/registry.c	\
	src/cjson/cJSON.c \

OUT = bluehats

build:
	$(CC) $(CFLAGS) $(SRCS) -o $(OUT) $(LIBS)

clean:
	rm -f $(OUT)