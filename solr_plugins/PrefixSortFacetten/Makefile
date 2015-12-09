# Locate directories
PROJECT_NAME=FacetPrefixSortComponent
JAR_FILE=FacetPrefixSortComponent-0.0.1.jar
SOLR_JAR_FILE=$(VUFIND_HOME)/solr/lib/FacetPrefixSortComponent.jar
VUFIND_SOLR=$(VUFIND_HOME)/solr
VUFIND_JETTY=$(VUFIND_SOLR)/jetty
BIN=bin
SRC=src
PACKAGE_DIRECOTRY=de/uni_tuebingen/ub/ixTheo/handler/component
LIB_DIRECTORIES=$(VUFIND_SOLR)/lib/*:$(VUFIND_JETTY)/lib/*:$(VUFIND_JETTY)/lib/ext/*:$(VUFIND_JETTY)/work/jetty-0.0.0.0-8080-solr.war-_solr-any-/webapp/WEB-INF/lib/*
JAVA_VERSION=1.7
JAVA_FILES=$(shell find -name *.java)


# Compiler
JAVAC=javac
JAVAFLAGS=-g:source,lines,vars -d $(BIN) -classpath $(BIN):$(LIB_DIRECTORIES) -sourcepath $(SRC) -source $(JAVA_VERSION) -target $(JAVA_VERSION)
COMPILE = $(JAVAC) $(JAVAFLAGS)

JAR=jar cvf 
JAR_FLAGS=

all: $(JAR_FILE)

$(JAR_FILE): $(JAVA_FILES)
	test -d bin || mkdir bin
	find -name '*.java' -print0 | xargs -0 $(COMPILE)	
	find $(BIN)/ -name '*.class' -print0 | xargs -0 $(JAR) $(JAR_FILE) $(JAR_FLAGS)  

install: $(JAR_FILE)
	cp -f $(JAR_FILE) $(SOLR_JAR_FILE)

clean:
	rm -f $(JAR_FILE)
	rm -rf $(BIN)
