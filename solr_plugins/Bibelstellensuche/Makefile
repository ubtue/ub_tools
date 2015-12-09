# Locate directories
VUFIND_HOME=/usr/local/vufind2
PROJECT_NAME=bibleRangeSearch
JAR_FILE=BibleRangeParser-1.0.0.jar
SOLR_JAR_FILE=$(VUFIND_HOME)/solr/lib/BibleRangeParser.jar
BIN=bin
SRC=src
PACKAGE_DIRECOTRY=de/uni_tuebingen/ub/ixTheo/$(PROJECT_NAME)
LIB_DIRECTORY=/usr/local/vufind2/solr/lib/*:/usr/local/vufind2/solr/jetty/lib/ext/*:/usr/local/vufind2/solr/jetty/work/jetty-0.0.0.0-8080-solr.war-_solr-any-/webapp/WEB-INF/lib/*

# Compiler
JAVAC=javac
JAVAFLAGS=-g -d $(BIN) -classpath $(BIN):$(LIB_DIRECTORY) -sourcepath $(SRC)
COMPILE = $(JAVAC) $(JAVAFLAGS)

JAR=jar cvf 
JAR_FLAGS=-C $(BIN)

bin: 
	test -d bin || mkdir bin
	$(COMPILE) $(SRC)/$(PACKAGE_DIRECOTRY)/*.java
	$(JAR) $(JAR_FILE) $(JAR_FLAGS) $(PACKAGE_DIRECOTRY)

install: bin
	cp -f $(JAR_FILE) $(SOLR_JAR_FILE)

clean:
	rm -f $(JAR_FILE)
	rm -rf $(BIN)
