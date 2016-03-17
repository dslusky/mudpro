BUILD_DATE ?= $(shell date +'%Y%m%d')

build:
	cd src; $(MAKE) $(MFLAGS)

package:
	DESTDIR=mudpro-$(VERSION)
	DESTFILE=mudpro-$(VERSION).tar.gz
	mkdir -p $(DESTDIR)
	cp src/mudpro $(DESTDIR)
	cp README COPYING CREDITS ChangeLog $(DESTDIR)
	cp -a profile $(DESTDIR)
	tar czf $(DESTFILE) $(DESTDIR)
	rm -rf $(DESTDIR)

release: build package
	VERSION ?= $(BUILD_DATE)

debug: build package
	VERSION ?= $(shell git rev-parse --short HEAD)

clean:
	cd src; $(MAKE) clean
	rm -f $(DESTFILE)
