CCC=g++
CCOPTS=-g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -O3 -c
LDOPTS=-O3

.PHONY: clean

%.o: %.cc
	$(CCC) $(CCOPTS) $<


libmarc.a: Subfields.o RegexMatcher.o Leader.o StringUtil.o DirectoryEntry.o MarcUtil.o util.o HtmlParser.o \
           StringUtil.o TextUtil.o Locale.o OCR.o MediaTypeUtil.o MapIO.o MediaTypeUtil.o HtmlParser.o \
           Downloader.o MarcGrepTokenizer.o MarcQueryParser.o SmartDownloader.o BibleReferenceParser.o \
           ExecUtil.o PdfUtil.o FileUtil.o IniFile.o TimeLimit.o TimerUtil.o ThreadManager.o
	@echo "Linking $@..."
	@ar crs $@ $^

FileUtil.o: FileUtil.h Compiler.h
PdfUtil.o: PdfUtil.h ExecUtil.h util.h FileUtil.h
ExecUtil.o: ExecUtil.h util.h
MapIO.o: MapIO.h StringUtil.h util.h
MediaTypeUtil.o: MediaTypeUtil.cc MediaTypeUtil.h
Locale.o: Locale.h
util.o: util.h
HtmlParser.o: HtmlParser.h Compiler.h StringUtil.h util.h
StringUtil.o: StringUtil.h Compiler.h
TextUtil.o: TextUtil.h Locale.h StringUtil.h Compiler.h HtmlParser.h RegexMatcher.h
Subfields.o: Subfields.h util.h
RegexMatcher.o: RegexMatcher.h util.h
Leader.o: Leader.h StringUtil.h
DirectoryEntry.o: DirectoryEntry.h StringUtil.h util.h
MarcUtil.o: MarcUtil.h DirectoryEntry.h Leader.h util.h Compiler.h Subfields.h
Downloader.o: Downloader.h util.h FileUtil.h
MarcGrepTokenizer.o: MarcGrepTokenizer.h StringUtil.h
MarcQueryParser.o: DirectoryEntry.h Leader.h MarcQueryParser.h MarcGrepTokenizer.h RegexMatcher.h StringUtil.h util.h
OCR.o: OCR.h ExecUtil.h FileUtil.h
SmartDownloader.o: SmartDownloader.h RegexMatcher.h Downloader.h util.h StringUtil.h MediaTypeUtil.h FileUtil.h \
                   TimeLimit.h
BibleReferenceParser.o: BibleReferenceParser.h StringUtil.h Locale.h util.h
IniFile.o: IniFile.h util.h FileUtil.h
TimeLimit.o: TimeLimit.h TimerUtil.h
TimerUtil.o: TimerUtil.h Compiler.h
ThreadManager.o: ThreadManager.h util.h

clean:
	rm -f *~ $(PROGS) *.o *.a
