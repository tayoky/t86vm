include config.mk

BUILDDIR   = build
SRCDIR     = src
INCLUDEDIR = include

VERSION = $(shell git describe --tags --always)

SRC = $(shell find $(SRCDIR) -maxdepth 1 -name "*.c")
OBJ = $(SRC:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

CFLAGS += -I$(INCLUDEDIR)
CFLAGS += -DT86VM_VERSION='"$(VERSION)"'

all : $(BUILDDIR)/t86vm

$(BUILDDIR)/t86vm : $(OBJ)
	@echo '[linking into $@]'
	@mkdir -p $(shell dirname $@)
	@$(CC) -o $@ $^ $(CFLAGS)


$(BUILDDIR)/%.o : $(SRCDIR)/%.c 
	@echo '[compiling $^]'
	@mkdir -p $(shell dirname $@)
	@$(CC) -o $@ -c $^ $(CFLAGS)


install : all
	@echo '[installing t86vm]'
	@mkdir -p $(PREFIX)/bin
	@cp $(BUILDDIR)/t86vm $(PREFIX)/bin/t86vm

uninstall :
	rm -f $(PREFIX)/bin/t86vm

clean :
	rm -fr build

config.mk :
	$(error run ./configure before running make)

.PHONY : all $(BUILDIR)/t86vm install uninstall clean
