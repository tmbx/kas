# Misc tools for web interfaces.
#

# Works for installing locally or creating a Debian package.
# make install				# Locally.
# make install DEBIANDIR=$(CURDIR)	# From debian rules.

# For Debian packages.
ifdef DEBIANDIR
	VIRTUALDIR=$(DEBIANDIR)/debian/$(DEBIANPACKAGE)
else
	DEBIANDIR=
	VIRTUALDIR=
endif

all:
	:

clean:
	# Remove development synlinks
	rm -f kas_cfg/kascfg/public/css/base \
		kas_cfg/kascfg/public/images/base \
		kas_cfg/kascfg/public/html/base \
		kas_cfg/kascfg/public/applets/base \
		kas_cfg/kascfg/public/javascripts/base

	rm -f kwmo/kwmo/public/css/base \
		kwmo/kwmo/public/images/base \
		kwmo/kwmo/public/html/base \
		kwmo/kwmo/public/applets/base \
		kwmo/kwmo/public/javascripts/base

	rm -f freemium/freemium/public/css/base \
		freemium/freemium/public/images/base \
		freemium/freemium/public/html/base \
		freemium/freemium/public/applets/base \
		freemium/freemium/public/javascripts/base

test:
	:

install-part:
	# Make sure the directories for all subpackages exist locally.
	mkdir -p config-stock/$(SUBPACKAGE)/
	mkdir -p python/$(SUBPACKAGE)/
	mkdir -p scripts/$(SUBPACKAGE)/
	mkdir -p templates/$(SUBPACKAGE)/

	# Create web files directory.
	mkdir -p $(VIRTUALDIR)/usr/share/teambox/web/

	# Copy non-web files.
	cp -rf --parent config-stock/$(SUBPACKAGE) $(VIRTUALDIR)/usr/share/teambox/web/
	cp -rf --parent python/$(SUBPACKAGE) $(VIRTUALDIR)/usr/share/teambox/web/
	cp -rf --parent scripts/$(SUBPACKAGE) $(VIRTUALDIR)/usr/share/teambox/web/
	cp -rf --parent templates/$(SUBPACKAGE) $(VIRTUALDIR)/usr/share/teambox/web/

ifeq ($(SUBPACKAGE), base)
	# Install common web files.
	mkdir -p www/applets/$(SUBPACKAGE)/
	mkdir -p www/css/$(SUBPACKAGE)/
	mkdir -p www/images/$(SUBPACKAGE)/
	mkdir -p www/javascripts/$(SUBPACKAGE)/
	mkdir -p www/html/$(SUBPACKAGE)/
	cp -rf --parent www/applets/$(SUBPACKAGE) $(VIRTUALDIR)/usr/share/teambox/web/
	cp -rf --parent www/css/$(SUBPACKAGE) $(VIRTUALDIR)/usr/share/teambox/web/
	cp -rf --parent www/images/$(SUBPACKAGE) $(VIRTUALDIR)/usr/share/teambox/web/
	cp -rf --parent www/javascripts/$(SUBPACKAGE) $(VIRTUALDIR)/usr/share/teambox/web/
	cp -rf --parent www/html/$(SUBPACKAGE) $(VIRTUALDIR)/usr/share/teambox/web/
endif

ifneq ($(SUBPACKAGE), base)
	# Install pylons application.
	cp -rf $(SUBPACKAGE) $(VIRTUALDIR)/usr/share/teambox/web/
endif


install:
	# Recall this makefile for all packages to install.
	make install-part DEBIANDIR=$(DEBIANDIR) SUBPACKAGE=base DEBIANPACKAGE=kas-web-base
	make install-part DEBIANDIR=$(DEBIANDIR) SUBPACKAGE=kas_cfg DEBIANPACKAGE=kas-cfg
	make install-part DEBIANDIR=$(DEBIANDIR) SUBPACKAGE=kwmo DEBIANPACKAGE=kwmo
	make install-part DEBIANDIR=$(DEBIANDIR) SUBPACKAGE=freemium DEBIANPACKAGE=freemium

