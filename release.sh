#!/bin/sh

cd src
	make clean
	make -f Makefile.rel
	strip -sx mudpro
	mv -f mudpro ..
	make clean
cd -

cd ..
tar cvzf MudPRO/mudpro-release.tar.gz \
	MudPRO/COPYING \
	MudPRO/CREDITS \
	MudPRO/ChangeLog \
	MudPRO/README \
	MudPRO/TODO \
	MudPRO/mudpro \
	MudPRO/profile \
	MudPRO/src \
	MudPRO/update-profile
cd -

rm -f mudpro
