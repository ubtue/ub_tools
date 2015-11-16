# Locate directories
VUFIND_HOME=/usr/local/vufind2
PROJECT_NAME=SolrmarcMixin
JAR_FILE=$(PROJECT_NAME)-1.0.0.jar
TARGET_JAR_FILE=$(VUFIND_HOME)/import/Tuelib$(PROJECT_NAME).jar
RESOURCES=resources
BIN=bin
SRC=src
PACKAGE_DIRECOTRY=de/unituebingen/ub/ubtools/solrmarcMixin
LIB_DIRECTORY=./lib/*

# Compiler
JAVAC=javac
JAVAFLAGS=-g -d $(BIN) -classpath $(BIN):$(LIB_DIRECTORY) -sourcepath $(SRC)
COMPILE = $(JAVAC) $(JAVAFLAGS)

JAR=jar cvf 
JAR_FLAGS=-C $(BIN)

CP=cp -f

all: compile

compile: 
	test -d bin || mkdir bin
	$(COMPILE) $(SRC)/$(PACKAGE_DIRECOTRY)/*.java
	$(CP) $(RESOURCES)/* $(BIN)
	$(JAR) $(JAR_FILE) $(JAR_FLAGS) $(PACKAGE_DIRECOTRY) bin/log4j.properties

install: compile
	$(CP) $(JAR_FILE) $(TARGET_JAR_FILE)

clean:
	rm -f $(JAR_FILE)
	rm -rf $(BIN)
