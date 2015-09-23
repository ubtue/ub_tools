CCC=g++
CCOPTS=-g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -march=native -Ilibstemmer/include -O3 \
       -pedantic -c
LDOPTS=-O3

.PHONY: clean

%.o: %.cc
	$(CCC) $(CCOPTS) $<


all: libstemmer/libstemmer.a libmarc.a

libmarc.a: Subfields.o RegexMatcher.o Leader.o StringUtil.o DirectoryEntry.o MarcUtil.o util.o HtmlParser.o \
           StringUtil.o TextUtil.o Locale.o OCR.o MediaTypeUtil.o MapIO.o MediaTypeUtil.o HtmlParser.o \
           Downloader.o MarcGrepTokenizer.o MarcQueryParser.o SmartDownloader.o BibleReferenceParser.o \
           ExecUtil.o PdfUtil.o FileUtil.o IniFile.o TimeLimit.o TimerUtil.o ThreadManager.o IdbPager.o \
           HtmlUtil.o Stemmer.o FileLocker.o DbConnection.o DbRow.o DbResultSet.o PHPUtil.o
	@echo "Linking $@..."
	@ar crs $@ $^

libstemmer/libstemmer.a: $(wildcard libstemmer/src_c/*.c) $(wildcard libstemmer/include/*.h)
	$(MAKE) -C libstemmer

BibleReferenceParser.o: BibleReferenceParser.h StringUtil.h Locale.h util.h
DbConnection.o: DbConnection.h DbResultSet.h
DbResultSet.o: DbResultSet.h DbRow.h
DbRow.o: DbRow.h
DirectoryEntry.o: DirectoryEntry.h StringUtil.h util.h
Downloader.o: Downloader.h util.h FileUtil.h ExecUtil.h
ExecUtil.o: ExecUtil.h StringUtil.h util.h
FileLocker.o: FileLocker.h
FileUtil.o: FileUtil.h Compiler.h
HtmlParser.o: HtmlParser.h Compiler.h StringUtil.h util.h
HtmlUtil.o: HtmlUtil.h
IdbPager.o: IdbPager.h TimeLimit.h Downloader.h StringUtil.h
MapIO.o: MapIO.h StringUtil.h util.h
MarcGrepTokenizer.o: MarcGrepTokenizer.h StringUtil.h
MarcQueryParser.o: DirectoryEntry.h Leader.h MarcQueryParser.h MarcGrepTokenizer.h RegexMatcher.h StringUtil.h util.h
MediaTypeUtil.o: MediaTypeUtil.cc MediaTypeUtil.h
Leader.o: Leader.h StringUtil.h
Locale.o: Locale.h
MarcUtil.o: MarcUtil.h DirectoryEntry.h Leader.h util.h Compiler.h Subfields.h StringUtil.h
PdfUtil.o: PdfUtil.h ExecUtil.h util.h FileUtil.h
PHPUtil.o: PHPUtil.h Compiler.h StringUtil.h
OCR.o: OCR.h ExecUtil.h FileUtil.h
RegexMatcher.o: RegexMatcher.h util.h
SmartDownloader.o: SmartDownloader.h RegexMatcher.h Downloader.h util.h StringUtil.h MediaTypeUtil.h FileUtil.h \
                   TimeLimit.h IdbPager.h
Stemmer.o: Stemmer.h libstemmer/libstemmer.a
StringUtil.o: StringUtil.h Compiler.h
Subfields.o: Subfields.h util.h
TextUtil.o: TextUtil.h Locale.h StringUtil.h Compiler.h HtmlParser.h RegexMatcher.h
IniFile.o: IniFile.h util.h FileUtil.h
ThreadManager.o: ThreadManager.h util.h
TimeLimit.o: TimeLimit.h TimerUtil.h
TimerUtil.o: TimerUtil.h Compiler.h
util.o: util.h

clean:
	rm -f *~ $(PROGS) *.o *.a && cd libstemmer && make clean && rm libstemmer.a
