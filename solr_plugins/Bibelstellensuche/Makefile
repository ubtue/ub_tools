# Locate directories
PROJECT_NAME=bibleRangeSearch
JAR_FILE=BibleRangeParser-1.0.0.jar
SOLR_JAR_FILE=$(VUFIND_HOME)/solr/lib/BibleRangeParser.jar
BIN=bin
SRC=src
PACKAGE_DIRECOTRY=de/uni_tuebingen/ub/ixTheo/$(PROJECT_NAME)
LIB_DIRECTORY=../lib/*

# Compiler
JAVAC=javac
JAVAFLAGS=-g -d $(BIN) -classpath $(BIN):$(LIB_DIRECTORY) -sourcepath $(SRC) -source 1.7 -target 1.7
COMPILE = $(JAVAC) $(JAVAFLAGS)

JAR=jar cvf 
JAR_FLAGS=-C $(BIN)

all:

compile: 
	test -d bin || mkdir bin
	$(COMPILE) $(SRC)/$(PACKAGE_DIRECOTRY)/*.java
	$(JAR) $(JAR_FILE) $(JAR_FLAGS) $(PACKAGE_DIRECOTRY)

install: compile
	cp -f $(JAR_FILE) $(SOLR_JAR_FILE)

clean:
	rm -f $(JAR_FILE)
	rm -rf $(BIN)
