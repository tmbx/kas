JC = javac
JAR = jar
JARSIGNER = jarsigner
JC_TARGET = 1.5
JC_SOURCE = 1.5
ARCHIVE = VncViewer.jar
KEYSTORE = ../code_signing_cert.p12
LIB_KAS_COMMON = ../web/www/applets/base/kas_common.jar
CLASSPATH = .:$(LIB_KAS_COMMON)

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
		`find . -name "*.java"`

	$(JAR) cf $(ARCHIVE) `find . -name "*.class"`

sign:
	$(JARSIGNER) \
	    -keystore $(KEYSTORE) \
	    -storetype "pkcs12" \
	    $(ARCHIVE) 1
	$(JARSIGNER) -verify $(ARCHIVE)

install:
	install -m 644 $(ARCHIVE) ../web/kwmo/kwmo/public/applets/$(ARCHIVE)

