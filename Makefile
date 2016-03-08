RELEASE=$(shell date +'%Y%m%d')
DESTDIR=mudpro-$(RELEASE)
DESTFILE=mudpro-$(RELEASE).tar.gz

build:
	cd src; $(MAKE) $(MFLAGS)

release: build
	mkdir -p $(DESTDIR)
	cp src/mudpro $(DESTDIR)
	cp README COPYING CREDITS ChangeLog $(DESTDIR)
	cp -a profile $(DESTDIR)
	tar czf $(DESTFILE) $(DESTDIR)
	rm -rf $(DESTDIR)

clean:
	cd src; $(MAKE) clean
	rm -f $(DESTFILE)
