include mkinc.mak
CFLAGS=-Iinclude -O3 -march=native
CFLAGS_AMD64=subst(-march=native,,$(CFLAGS))

src_c/%_amd64.o: src_c/%.c
	$(CC) $(AMD64_CCOPTS) $< -c -o $@
runtime/%_amd64.o: runtime/%.c
	$(CC) $(AMD64_CCOPTS) $< -c -o $@
libstemmer/%_amd64.o: libstemmer/%.c
	$(CC) $(AMD64_CCOPTS) $< -c -o $@

all: libstemmer.a stemwords

libstemmer.a: $(snowball_sources:.c=.o)
	$(AR) -cru $@ $^

libstemmer_amd64.a: $(snowball_sources:.c=_amd64.o)
	$(AR) -cru $@ $^

stemwords: examples/stemwords.o libstemmer.a
	$(CC) -o $@ $^

clean:
	rm -f stemwords *.o src_c/*.o runtime/*.o libstemmer/*.o
