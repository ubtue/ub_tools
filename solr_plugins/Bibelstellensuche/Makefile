# Locate directories
VUFIND_HOME=/usr/local/vufind2
PROJECT_NAME=bibleRangeSearch
JAR_FILE=BibleRangeParser-1.0.0.jar
SOLR_JAR_FILE=$(VUFIND_HOME)/solr/vufind/jars/BibleRangeParser.jar
BIN=bin
SRC=src
PACKAGE_DIRECOTRY=de/uni_tuebingen/ub/ixTheo/$(PROJECT_NAME)

EMPTY :=
SPACE := $(EMPTY) $(EMPTY)
LIB_DIRECTORY_0=$(subst $(SPACE),:,$(wildcard /usr/local/vufind2/solr/lib/*.jar))
LIB_DIRECTORY_1=$(subst $(SPACE),:,$(wildcard /usr/local/vufind2/solr/vendor/server/lib/*.jar))
LIB_DIRECTORY_2=$(subst $(SPACE),:,$(wildcard /usr/local/vufind2/solr/vendor/server/lib/ext/*.jar))
LIB_DIRECTORY_3=$(subst $(SPACE),:,$(wildcard /usr/local/vufind2/solr/vendor/server/solr-webapp/webapp/WEB-INF/lib/*.jar))

LIB_DIRECTORY=$(LIB_DIRECTORY_0):$(LIB_DIRECTORY_1):$(LIB_DIRECTORY_2):$(LIB_DIRECTORY_3)

$(info $(LIB_DIRECTORY))

# Compiler
JAVAC=javac
JAVAFLAGS=-g -d $(BIN) -classpath $(BIN):$(LIB_DIRECTORY) -sourcepath $(SRC) -source 1.7 -target 1.7
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
