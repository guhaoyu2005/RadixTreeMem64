CC       = clang
CFLAGS   = -Wall -I/usr/local/include -g -x c 
LDFLAGS  = -L/usr/local/lib 
OBJS = 		test.o rtree.o
TARGET   = rtree

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%o: %.c
	$(CC) $(CFLAGS)  -o $@ -c $<

clean:
	rm *.o
	rm $(TARGET)
