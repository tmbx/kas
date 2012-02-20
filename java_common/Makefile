JC = javac
JAR = jar
JARSIGNER = jarsigner
JC_TARGET = 1.5
JC_SOURCE = 1.5
ARCHIVE = kas_common.jar
KEYSTORE = ../code_signing_cert.p12
CLASSPATH = .:$(LIVECONNECTLIB)
LIVECONNECTLIB = lib/plugin.jar # taken from a recent sun jre (debian jre packages don't seem to have this)

all: clean build sign

clean:
	find . -name "*.class" -exec rm -f {} \;
	rm -f $(ARCHIVE)

build:
	$(JC) \
		-classpath $(CLASSPATH) \
    		-source $(JC_SOURCE) \
    		-target $(JC_TARGET) \
    		-deprecation \
            -Xlint:unchecked \
    		`find . -name "*.java"`

	$(JAR) cf $(ARCHIVE) `find . -name "*.class"`

sign:
	$(JARSIGNER) \
	    -keystore $(KEYSTORE) \
	    -storetype "pkcs12" \
	    $(ARCHIVE) 1
	$(JARSIGNER) -verify $(ARCHIVE)

install:
	install -m 644 $(ARCHIVE) ../web/www/applets/base/$(ARCHIVE)

