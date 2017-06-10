
DESTDIR="/"

auto-rele: auto-rele.c auto-rele.h auto-rele-defaults.h
	gcc auto-rele.c -liniparser -lwiringPi -lrt -lsystemd -o auto-rele


install: auto-rele auto-rele.conf auto-rele.service

	install -Dm 755 auto-rele "$(DESTDIR)/usr/bin/auto-rele"
