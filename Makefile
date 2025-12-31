CC = i686-w64-mingw32-gcc
CFLAGS = -std=c99 -Wall -Wextra -Isrc -D_WIN32_WINNT=0x0501
# Static linking for OpenSSL and runtime libraries for Windows XP compatibility
LDFLAGS = -static -static-libgcc -mwindows -lcomctl32 -lgdi32 -lwininet -lws2_32 -lole32 -loleaut32 -luuid -lssl -lcrypto -lcrypt32 -lz

CORE_SRC = src/core/dom.c src/core/html.c src/core/style.c src/core/layout.c src/core/log.c
SRC = src/main.c src/ui/window.c src/ui/history.c src/ui/bookmarks.c src/ui/render.c src/ui/form.c src/network/http.c src/network/gemini.c src/network/loader.c src/network/protocol.c src/network/tls.c $(CORE_SRC)
OBJ = $(SRC:.c=.o)
TARGET = gem32.exe
.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: tests/all_tests.c tests/test_network.c tests/test_core.c tests/test_ui.c \
      src/network/tls.c src/network/http.c src/network/gemini.c \
      src/network/protocol.c $(CORE_SRC)
	$(CC) $(CFLAGS) -DTEST_BUILD -o gem32-tests.exe $^ $(LDFLAGS) -mconsole
	./gem32-tests.exe

clean:
	rm -f $(OBJ) $(TARGET) gem32-tests.exe
