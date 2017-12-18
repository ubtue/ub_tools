include mkinc.mak
CFLAGS=-Iinclude -O3 -march=native

all: libstemmer.a stemwords

libstemmer.a: $(snowball_sources:.c=.o)
	$(AR) -cruU $@ $^

stemwords: examples/stemwords.o libstemmer.a
	$(CC) --static -o $@ $^

clean:
	rm -f stemwords *.o src_c/*.o runtime/*.o libstemmer/*.o
