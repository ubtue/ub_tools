LIB         = .
include ../Makefile.inc
INC         = ./include
SRC         = src
OBJ         = objs
STATIC_OBJ  = static_objs
SRCS        = $(SRC)/*.cc
TEMP        = $(addprefix $(OBJ)/,$(notdir $(wildcard $(SRC)/*.cc)))
STATIC_TEMP = $(addprefix $(STATIC_OBJ)/,$(notdir $(wildcard $(SRC)/*.cc)))
OBJS        = $(TEMP:.cc=.o)
STATIC_OBJS = $(STATIC_TEMP:.cc=_static.o)
MAKE_DEPS   = iViaCore-mkdep

.PHONY: clean
.PRECIOUS: $(OBJ)/%.o $(STATIC_OBJ)/%_static.o

# Rules for building:
$(OBJ)/%.o: $(SRC)/%.cc Makefile
	@echo "Compiling $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) $(CCCFLAGS) $< -c -o $@
$(STATIC_OBJ)/%_static.o: $(SRC)/%.cc Makefile
	@echo "Compiling static $< $(OPTIMISATIONS_STATE)..."
	@$(CCC) -static $(filter-out -march=native, $(CCCFLAGS)) $< -c -o $@

all: /usr/local/bin/iViaCore-mkdep .deps libubtue.a

libubtue.a: $(OBJS) Makefile
	@echo "Generating $@..."
	@ar crs $@ $(OBJS)

libubtue_static.a: $(STATIC_OBJS) Makefile
	@echo "Generating $@..."
	@ar crs $@ $(STATIC_OBJS)

/usr/local/bin/iViaCore-mkdep:
	$(MAKE) -C mkdep install

-include .deps
.deps: $(SRCS) $(INC)/*.h Makefile
	$(MAKE_DEPS) -I $(INC) $(SRCS)

-include .static_deps
.static_deps: $(SRCS) $(INC)/*.h Makefile
	$(MAKE_DEPS) --static-objects -I $(INC) $(SRCS)

clean:
	rm -f *~ $(OBJ)/*.o $(STATIC_OBJ)/*.o *.a .deps .static_deps
	$(MAKE) -C mkdep clean

