# Locate directories
VUFIND_HOME=/usr/local/vufind
PROJECT_NAME=SolrmarcMixin
JAR_FILE=$(PROJECT_NAME)-1.0.0.jar
TARGET_JAR_FILE=$(VUFIND_HOME)/import/lib/Tuelib$(PROJECT_NAME).jar
RESOURCES=resources
BIN=bin
SRC=src
PACKAGE_DIRECOTRY=de/unituebingen/ub/ubtools/solrmarcMixin
LIB_DIRECTORY=lib/*:/usr/local/vufind/solr/lib/*:/usr/local/vufind/solr/jetty/lib/ext/*:/usr/local/vufind/solr/jetty/work/jetty-0.0.0.0-8080-solr.war-_solr-any-/webapp/WEB-INF/lib/*
JAVA_VERSION=1.7

# Compiler
JAVAC=javac
JAVAFLAGS=-g -Xlint:unchecked -Xlint:deprecation -d $(BIN) -classpath $(BIN):$(LIB_DIRECTORY) -sourcepath $(SRC) \
          -source $(JAVA_VERSION) -target $(JAVA_VERSION)
COMPILE = $(JAVAC) $(JAVAFLAGS)

JAR=jar cvf 
JAR_FLAGS=-C $(BIN)

CP=cp -f

$(JAR_FILE): $(SRC)/$(PACKAGE_DIRECOTRY)/*.java
	test -d bin || mkdir bin
	$(COMPILE) $(SRC)/$(PACKAGE_DIRECOTRY)/*.java
	$(CP) $(RESOURCES)/* $(BIN)
	$(JAR) $(JAR_FILE) $(JAR_FLAGS) $(PACKAGE_DIRECOTRY) bin/log4j.properties

install: $(JAR_FILE)
	$(CP) $(JAR_FILE) $(TARGET_JAR_FILE)

clean:
	rm -f $(JAR_FILE)
	rm -rf $(BIN)
