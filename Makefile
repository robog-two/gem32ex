CC = i686-w64-mingw32-gcc
CFLAGS = -std=c99 -Wall -Wextra -Isrc -D_WIN32_WINNT=0x0501
LDFLAGS = -mwindows -lcomctl32 -lgdi32

SRC = src/main.c src/ui/window.c
OBJ = $(SRC:.c=.o)
TARGET = gem32.exe

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
