BUILD_DATE ?= $(shell date +'%Y%m%d')
VERSION ?= $(BUILD_DATE)
DESTDIR=mudpro-$(VERSION)
DESTFILE=mudpro-$(VERSION).tar.gz

build:
	cd src; $(MAKE) $(MFLAGS)

package:
	mkdir -p $(DESTDIR)
	cp src/mudpro $(DESTDIR)
	cp README COPYING CREDITS ChangeLog $(DESTDIR)
	cp -a profile $(DESTDIR)
	tar czf $(DESTFILE) $(DESTDIR)
	rm -rf $(DESTDIR)

release: build package

debug: build package

clean:
	cd src; $(MAKE) clean
	rm -f $(DESTFILE)
