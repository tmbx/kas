#
# KAS python library
#

# Prefix for installing lib... so we can use the same makefile for local
# installation and debian package creation.
ifndef DESTDIR
	DESTDIR=
endif

# Python modules to install
PYTHON_MODULES=kanp.py kcdpg.py tcp_client.py kcgi.py kfs_lib.py kcd_lib.py kcd_client.py ksamba.py

all:
	:

clean:
	rm -f *.pyc

install:
	# Install kcdshell
	mkdir -p $(DESTDIR)/usr/bin/
	install -m 755 kcdshell $(DESTDIR)/usr/bin/kcdshell

	# Install kcdshell stock config
	mkdir -p $(DESTDIR)/usr/share/teambox/kas-python/config-stock/
	install -m644 config-stock/kcdshell.ini $(DESTDIR)/usr/share/teambox/kas-python/config-stock/

	# Install Python modules.
	mkdir -p $(DESTDIR)/usr/share/python-support/kas/
	for i in $(PYTHON_MODULES); do \
	    install -m644 $$i $(DESTDIR)/usr/share/python-support/kas/;\
	done
	
	# If we're installing locally (not building a debian package),
	# we update python modules.
ifndef DEBIAN
	update-python-modules kas
endif

