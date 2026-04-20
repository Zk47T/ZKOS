SUBDIRS = $(dir $(shell find . -mindepth 2 -name Makefile))

all:
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir; done

clean:
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
