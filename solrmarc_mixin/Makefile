# Locate directories
VUFIND_HOME=/usr/local/vufind
PROJECT_NAME=SolrmarcMixin
JAR_FILE=$(PROJECT_NAME)-1.0.0.jar
TARGET_JAR_FILE=$(VUFIND_HOME)/import/lib/Tuelib$(PROJECT_NAME).jar
RESOURCES=resources
BIN=bin
SRC=src
PACKAGE_DIRECTORY=de/unituebingen/ub/ubtools/solrmarcMixin
LIB_DIRECTORY=lib/*:/usr/local/vufind/solr/lib/*:/usr/local/vufind/solr/jetty/lib/ext/*:/usr/local/vufind/solr/jetty/work/jetty-0.0.0.0-8080-solr.war-_solr-any-/webapp/WEB-INF/lib/*:/usr/local/vufind/import/lib/*:/usr/local/vufind/import/lib_local/*
JAVA_VERSION=1.8

# Compiler
JAVAC=javac
JAVAFLAGS=-g -Xlint:unchecked -Xlint:deprecation -d $(BIN) -classpath $(BIN):$(LIB_DIRECTORY) -sourcepath $(SRC):/usr/local/vufind/import/index_java/src \
          -source $(JAVA_VERSION) -target $(JAVA_VERSION)
COMPILE = $(JAVAC) $(JAVAFLAGS)

JAR=jar cvf 
JAR_FLAGS=-C $(BIN)

CP=cp -f

$(JAR_FILE): $(SRC)/$(PACKAGE_DIRECTORY)/*.java
	test -d bin || mkdir bin
	$(COMPILE) $(SRC)/$(PACKAGE_DIRECTORY)/*.java
	$(CP) $(RESOURCES)/* $(BIN)
	$(JAR) $(JAR_FILE) $(JAR_FLAGS) $(PACKAGE_DIRECTORY) bin/log4j.properties

install: $(JAR_FILE)
	$(CP) $(JAR_FILE) $(TARGET_JAR_FILE)

clean:
	rm -f $(JAR_FILE)
	rm -rf $(BIN)
