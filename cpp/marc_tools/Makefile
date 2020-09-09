LIB          := ../lib
INC          = $(LIB)/include
STATIC_OBJ   = .
STATIC_PROGS = $(patsubst %.cc,static_executables/%,$(wildcard *.cc))
include ../Makefile.inc


.PHONY: all .deps .static_deps debs install clean

all: .deps $(PROGS)
all_static: .static_deps $(STATIC_PROGS)

%.o: %.cc Makefile
	@echo "Compiling $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) $(CCCFLAGS) $< -c

%_static.o: %.cc Makefile
	@echo "Compiling static $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) -static $(filter-out -march=native, $(CCCFLAGS)) $< -c -o $@

$(PROGS): % : %.o ../lib/libubtue.a Makefile
	@echo "Linking $@..."
	@$(CCC) -rdynamic $(LD_FLAGS) $< -o $@ $(LIBS)

$(STATIC_PROGS): static_executables/% : %_static.o ../lib/libubtue_static.a Makefile
	@echo "Linking static $@..."
	@$(CCC) $(LD_FLAGS) -Wl,-Bstatic -L$(LIB) -lubtue_static -lstemmer -lpcre -lmagic \
                 -larchive -L/usr/lib/pristine-tar/suse-bzip2/ -lbz2 -lcurl -lnghttp2 -lidn2 \
                 -lmysqlclient -lrt -llz4 -llzma -llzo2 -lxml2 -lz -lssl -lcrypto -lldap -llber -lpsl -lrtmp -lsasl2 \
                 -lgnutls -lgcrypt -lgmp -lnettle -lunistring -luuid -llept -ltesseract -lsqlite3 -lxerces-c -lxml2 -lgss -ltasn1 -lshishi \
                 -lpthread -Wl,-Bdynamic -ldl -lc $< -o $@

debs: $(PROGS)
	for prog in $(PROGS); do \
	    build_package --deb --output-directory=debs $$prog description \
	                  elasticsearch libc6-i386 lib32stdc++6 lib32gcc1 pristine-tar; \
	done
	echo -n "Built on: " > debs/README
	lsb_release --description --short >> debs/README

-include .deps
.deps: *.cc $(INC)/*.h Makefile
	$(MAKE_DEPS) -I $(INC) *.cc

-include .static_deps
.static_deps: *.cc $(INC)/*.h Makefile
	$(MAKE_DEPS) --static-objects -I $(INC) *.cc

../lib/libubtue.a: $(wildcard ../lib/src/*.cc) $(wildcard ../lib/include/*.h)
	$(MAKE) -C ../lib

../lib/libubtue_static.a: $(wildcard ../lib/src/*.cc) $(wildcard ../lib/include/*.h)
	$(MAKE) -C ../lib libubtue_static.a

install: $(PROGS)
	@echo "Installing programs..."
	cp $(PROGS) marc_grep_multiple.sh /usr/local/bin/

clean:
	rm -f *.o *~ $(PROGS) $(STATIC_PROGS) .deps .static_deps debs/*.deb
