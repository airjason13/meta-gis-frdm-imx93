CFLAGS := -Wall -Wextra -O2
LDLIBS := -lgpiod -lpthread

TARGET := v-gpio-detect
SRC := v-gpio-detect.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(TARGET)
