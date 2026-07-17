CC ?= gcc
CFLAGS := -Wall -O2

TARGET := jbd4040_reg
SRCS   := jbd4040_reg.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

.PHONY: all clean
