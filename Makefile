# You can change zig cc to gcc or clang if you wish. I prefer zig due to easy cross compilation.
# For example you can cross compile to any linux musl with:
# zig cc -target x86-linux-musl
# You can compile to windows with:
# zig cc -target x86_64-windows-gnu
# There are many others - see `zig targets` and checkout ziglang.org!
# If you dont want to use zig, just change CC to CC = gcc
CC = zig cc
CFLAGS = -Wall -Wextra -O2
TARGET = patcher

all: $(TARGET)

$(TARGET): patcher.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean