# Locate directories
VUFIND_HOME=/usr/local/vufind2
PROJECT_NAME=keywordChainSearch
JAR_FILE=KeywordChainParser-0.0.1.jar
SOLR_JAR_FILE=$(VUFIND_HOME)/solr/vufind/jars/KeywordChainParser.jar
BIN=bin
SRC=src
PACKAGE_DIRECOTRY=de/uni_tuebingen/ub/ixTheo/$(PROJECT_NAME)
LIB_DIRECTORY=/usr/local/vufind2/solr/vendor/server/lib/*:/usr/local/vufind2/solrvendor/server/lib/ext/*:/usr/local/vufind2/solr/vendor/server/solr-webapp/webapp/WEB-INF/lib/*
JAVA_VERSION=1.7

# Compiler
JAVAC=javac
JAVAFLAGS=-g -d $(BIN) -classpath $(BIN):$(LIB_DIRECTORY) -sourcepath $(SRC) -source $(JAVA_VERSION) -target $(JAVA_VERSION)
COMPILE = $(JAVAC) $(JAVAFLAGS)

JAR=jar cvf 
JAR_FLAGS=-C $(BIN)

$(JAR_FILE): $(SRC)/$(PACKAGE_DIRECOTRY)/*.java
	test -d bin || mkdir bin
	$(COMPILE) $(SRC)/$(PACKAGE_DIRECOTRY)/*.java
	$(JAR) $(JAR_FILE) $(JAR_FLAGS) $(PACKAGE_DIRECOTRY)

install: $(JAR_FILE)
	cp -f $(JAR_FILE) $(SOLR_JAR_FILE)

clean:
	rm -f $(JAR_FILE)
	rm -rf $(BIN)
