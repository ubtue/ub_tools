PROJECT_NAME=RangedSearches
JAR_FILE=$(PROJECT_NAME).jar

EMPTY :=
SPACE := $(EMPTY) $(EMPTY)
LIB_DIRECTORY_1=$(subst $(SPACE),:,$(wildcard /usr/local/vufind/solr/lib/*.jar))
LIB_DIRECTORY_2=$(subst $(SPACE),:,$(wildcard /usr/local/vufind/solr/vendor/server/lib/*.jar))
LIB_DIRECTORY_3=$(subst $(SPACE),:,$(wildcard /usr/local/vufind/solr/vendor/server/lib/ext/*.jar))
LIB_DIRECTORY_4=$(subst $(SPACE),:,$(wildcard /usr/local/vufind/solr/vendor/server/solr-webapp/webapp/WEB-INF/lib/*.jar))

LIB_DIRECTORY=$(LIB_DIRECTORY_1):$(LIB_DIRECTORY_2):$(LIB_DIRECTORY_3):$(LIB_DIRECTORY_4)

$(info $(LIB_DIRECTORY))

# Compiler
JAVAC=javac
JAVAFLAGS=-g -d bin -classpath bin:$(LIB_DIRECTORY) -sourcepath src
COMPILE = $(JAVAC) $(JAVAFLAGS)

$(JAR_FILE): src/de/uni_tuebingen/ub/ixTheo/*/*.java Makefile
	test -d bin || mkdir bin
	$(COMPILE) src/de/uni_tuebingen/ub/ixTheo/*/*.java
	jar cvf $(JAR_FILE) \
	    -C bin de/uni_tuebingen/ub/ixTheo/rangeSearch \
	    -C bin de/uni_tuebingen/ub/ixTheo/bibleRangeSearch \
	    -C bin de/uni_tuebingen/ub/ixTheo/canonesRangeSearch \
	    -C bin de/uni_tuebingen/ub/ixTheo/timeAspectRangeSearch \

install: $(JAR_FILE)
	cp -f $(JAR_FILE) /usr/local/vufind/solr/vufind/jars/

clean:
	rm -f $(JAR_FILE)
	rm -rf bin
