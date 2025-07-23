MODULE = libutildbg
LIB = $(MAKE_DIR)/libs/$(MODULE).a

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o, $(SRCS))

$(LIB): $(OBJS)
	mkdir -p $(MAKE_DIR)/libs
	echo "LIB = $(LIB)"
	$(AR) cr $@ $^
	echo "Archive $(notdir $@)"

$(OBJS): $(SRCS)
	$(CC) -DDEBUG -D_REENTRANT -Wall -lpthread -c $^
	echo "error CFLAGS :" $(CFLAGS)
	#echo " CC $(CFLAGS) $(OBJS)"


.PHONY:clean
clean:
	$(RM) -f $(LIB) $(OBJS)
	$(RM) -f *expand
	echo "Remove Objects: $(OBJS)"
	echo "Remove Objects: $(notdir $(LIB))"
