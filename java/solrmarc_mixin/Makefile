# Locate directories
VUFIND_HOME=/usr/local/vufind
PROJECT_NAME=SolrmarcMixin
JAR_FILE=$(PROJECT_NAME)-1.0.0.jar
TARGET_JAR_FILE=$(VUFIND_HOME)/import/lib/Tuelib$(PROJECT_NAME).jar
RESOURCES=resources
BIN=bin
SRC=src
VUFIND_SRC=/usr/local/vufind/import/index_java/src
PACKAGE_DIRECTORY=de/unituebingen/ub/ubtools/solrmarcMixin
PACKAGE_DIRECTORY_VUFIND=org/vufind/index
PACKAGE_DIRECTORY_TUEFIND=org/tuefind/index
LIB_DIRECTORY=lib/*:/usr/local/vufind/solr/lib/*:/usr/local/vufind/solr/jetty/lib/ext/*:/usr/local/vufind/solr/jetty/work/jetty-0.0.0.0-8983-solr.war-_solr-any-/webapp/WEB-INF/lib/*:/usr/local/vufind/import/lib/*:/usr/local/vufind/import/lib_local/*:/usr/local/ub_tools/java/lib/*:/usr/local/vufind/solr/vendor/dist/solrj-lib/*:/usr/local/vufind/solr/vendor/server/solr-webapp/webapp/WEB-INF/lib/*
JAVA_VERSION=1.8

# Compiler
JAVAC=javac
JAVAFLAGS=-g -Xlint:unchecked -Xlint:deprecation -d $(BIN) -classpath $(BIN):$(LIB_DIRECTORY) -sourcepath $(SRC):$(VUFIND_SRC) \
          -source $(JAVA_VERSION) -target $(JAVA_VERSION)
COMPILE = $(JAVAC) $(JAVAFLAGS)

JAR=jar cvf
JAR_FLAGS=-C $(BIN)

CP=cp -f

$(JAR_FILE): $(SRC)/$(PACKAGE_DIRECTORY)/*.java $(VUFIND_SRC)/$(PACKAGE_DIRECTORY_VUFIND)/*.java $(VUFIND_SRC)/$(PACKAGE_DIRECTORY_TUEFIND)/*.java Makefile
	test -d bin || mkdir bin
	$(COMPILE) $(SRC)/$(PACKAGE_DIRECTORY)/*.java $(VUFIND_SRC)/$(PACKAGE_DIRECTORY_VUFIND)/*.java $(VUFIND_SRC)/$(PACKAGE_DIRECTORY_TUEFIND)/*.java
	$(CP) $(RESOURCES)/* $(BIN)
	$(JAR) $(JAR_FILE) $(JAR_FLAGS) $(PACKAGE_DIRECTORY) bin/$(PACKAGE_DIRECTORY_VUFIND) bin/$(PACKAGE_DIRECTORY_TUEFIND) bin/log4j.properties

install: $(JAR_FILE) Makefile
	$(CP) $(JAR_FILE) $(TARGET_JAR_FILE)

clean:
	rm -f $(JAR_FILE)
	rm -rf $(BIN)
