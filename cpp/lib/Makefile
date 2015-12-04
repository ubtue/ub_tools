CCC=g++
CCOPTS=-g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -march=native -Ilibstemmer/include -O3 \
       -pedantic -I/usr/include/libxml2 -DETC_DIR='"/var/lib/tuelib"' -c
LDOPTS=-O3

.PHONY: clean

%.o: %.cc
	$(CCC) $(CCOPTS) $<


all: libstemmer/libstemmer.a libmarc.a

libmarc.a: Subfields.o RegexMatcher.o Leader.o StringUtil.o DirectoryEntry.o MarcUtil.o util.o HtmlParser.o \
           StringUtil.o TextUtil.o Locale.o OCR.o MediaTypeUtil.o MapIO.o MediaTypeUtil.o HtmlParser.o \
           Downloader.o MarcGrepTokenizer.o MarcQueryParser.o SmartDownloader.o BibleReferenceParser.o \
           ExecUtil.o PdfUtil.o FileUtil.o IniFile.o TimeLimit.o TimerUtil.o ThreadManager.o IdbPager.o \
           HtmlUtil.o Stemmer.o FileLocker.o DbConnection.o DbRow.o DbResultSet.o PHPUtil.o EmailSender.o \
	   UrlUtil.o WebUtil.o XmlParser.o GzStream.o SocketUtil.o SslConnection.o FileDescriptor.o DnsUtil.o \
           NetUtil.o DnsServerAndPool.o Resolver.o Logger.o File.o HttpHeader.o TimeUtil.o PerlCompatRegExp.o \
           Url.o ThreadSafeLogger.o ThreadUtil.o MiscUtil.o
	@echo "Linking $@..."
	@ar crs $@ $^

libstemmer/libstemmer.a: $(wildcard libstemmer/src_c/*.c) $(wildcard libstemmer/include/*.h)
	$(MAKE) -C libstemmer

BibleReferenceParser.o: BibleReferenceParser.h StringUtil.h Locale.h util.h
DbConnection.o: DbConnection.h DbResultSet.h
DbResultSet.o: DbResultSet.h DbRow.h
DbRow.o: DbRow.h
DirectoryEntry.o: DirectoryEntry.h StringUtil.h util.h
DnsServerAndPool.o: Compiler.h DnsServerAndPool.h FileUtil.h Logger.h NetUtil.h Resolver.h SocketUtil.h TimeUtil.h util.h
DnsUtil.o: DnsServerAndPool.h DnsUtil.h NetUtil.h StringUtil.h TimerUtil.h TimeUtil.h Url.h
EmailSender.o: EmailSender.h FileUtil.h Compiler.h ExecUtil.h util.h
Downloader.o: Downloader.h util.h FileUtil.h ExecUtil.h
ExecUtil.o: ExecUtil.h StringUtil.h util.h
File.o: Compiler.h File.h FileUtil.h Logger.h ThreadUtil.h TimeUtil.h
FileDescriptor.o: Compiler.h FileDescriptor.h
FileLocker.o: FileLocker.h
FileUtil.o: Compiler.h FileUtil.h StringUtil.h
GzStream.o: GzStream.h Compiler.h
HtmlParser.o: HtmlParser.h Compiler.h StringUtil.h util.h
HtmlUtil.o: HtmlUtil.h
HttpHeader.o: HttpHeader.h MediaTypeUtil.h StringUtil.h TimeUtil.h
IdbPager.o: IdbPager.h TimeLimit.h Downloader.h StringUtil.h
IniFile.o: IniFile.h util.h FileUtil.h
Logger.o: File.h FileUtil.h Logger.h ThreadUtil.h TimeUtil.h
MapIO.o: MapIO.h StringUtil.h util.h
MarcGrepTokenizer.o: MarcGrepTokenizer.h StringUtil.h
MarcQueryParser.o: DirectoryEntry.h Leader.h MarcQueryParser.h MarcGrepTokenizer.h RegexMatcher.h StringUtil.h util.h
MediaTypeUtil.o: MediaTypeUtil.cc MediaTypeUtil.h
MiscUtil.o: MiscUtil.h util.h
NetUtil.o: NetUtil.h SList.h StringUtil.h
Leader.o: Leader.h StringUtil.h
Locale.o: Locale.h
Logger.o: File.h FileUtil.h Logger.h TimeUtil.h ThreadUtil.h util.h
MarcUtil.o: MarcUtil.h DirectoryEntry.h FileUtil.h Leader.h util.h Compiler.h Subfields.h StringUtil.h
PdfUtil.o: PdfUtil.h ExecUtil.h util.h FileUtil.h
PHPUtil.o: PHPUtil.h Compiler.h StringUtil.h
OCR.o: OCR.h ExecUtil.h FileUtil.h
PerlCompatRegExp.o: PerlCompatRegExp.h StringUtil.h
RegexMatcher.o: RegexMatcher.h Compiler.h util.h
Resolver.o: Resolver.h TimeLimit.h ThreadSafeLogger.h
SmartDownloader.o: SmartDownloader.h RegexMatcher.h Downloader.h util.h StringUtil.h MediaTypeUtil.h FileUtil.h \
                   TimeLimit.h IdbPager.h
SocketUtil.o: DnsUtil.h FileDescriptor.h SslConnection.h SocketUtil.h TimeLimit.h
SslConnection.o: Compiler.h TimeUtil.h SslConnection.h
Stemmer.o: Stemmer.h libstemmer/libstemmer.a
StringUtil.o: StringUtil.h Compiler.h
Subfields.o: Subfields.h util.h
TextUtil.o: TextUtil.h Locale.h StringUtil.h Compiler.h HtmlParser.h RegexMatcher.h
ThreadManager.o: ThreadManager.h util.h
ThreadSafeLogger.o: Logger.h ThreadSafeLogger.h
ThreadUtil.o: File.h Logger.h ThreadUtil.h TimeUtil.h
TimeLimit.o: TimeLimit.h TimerUtil.h
TimerUtil.o: TimerUtil.h Compiler.h
TimeUtil.o: PerlCompatRegExp.h TimeUtil.h
Url.o: Url.h
UrlUtil.o: Compiler.h IniFile.h MiscUtil.h PerlCompatRegExp.h SList.h StringUtil.h Url.h UrlUtil.h
util.o: util.h
WebUtil.o: Compiler.h FileDescriptor.h HttpHeader.h SocketUtil.h StringUtil.h TextUtil.h TimeLimit.h UrlUtil.h WebUtil.h
XmlParser.o: XmlParser.h StringUtil.h

clean:
	rm -f *~ $(PROGS) *.o *.a && cd libstemmer && make clean && rm -f libstemmer.a
