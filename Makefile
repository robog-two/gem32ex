CC = i686-w64-mingw32-gcc
CFLAGS = -std=c99 -Wall -Wextra -Isrc -D_WIN32_WINNT=0x0501
LDFLAGS = -mwindows -lcomctl32 -lgdi32 -lwininet -lws2_32 -lole32 -loleaut32 -luuid -lsecur32 -lcrypt32

CORE_SRC = src/core/dom.c src/core/html.c src/core/style.c src/core/layout.c src/core/log.c
SRC = src/main.c src/ui/window.c src/ui/history.c src/ui/bookmarks.c src/ui/render.c src/ui/form.c src/network/http.c src/network/gemini.c src/network/loader.c src/network/protocol.c src/network/tls.c $(CORE_SRC)
OBJ = $(SRC:.c=.o)
TARGET = gem32.exe
TEST_TARGET = gem32-tests.exe
CORE_TEST_TARGET = gem32-core-tests.exe

all: $(TARGET) $(TEST_TARGET) $(CORE_TEST_TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TEST_TARGET): tests/test_network.c src/network/tls.c src/network/http.c src/network/gemini.c src/core/log.c src/network/protocol.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -mconsole

$(CORE_TEST_TARGET): tests/test_core.c $(CORE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -mconsole

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(CORE_SRC) tests/test_html.c
	gcc -Isrc -o test_html tests/test_html.c $(CORE_SRC)
	./test_html

clean:
	rm -f $(OBJ) $(TARGET)
