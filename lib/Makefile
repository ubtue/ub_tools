CCC=g++
CCOPTS=-g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -O3 -c
LDOPTS=-O3

.PHONY: clean

%.o: %.cc
	$(CCC) $(CCOPTS) $<


libmarc.a: Subfields.o RegexMatcher.o Leader.o StringUtil.o DirectoryEntry.o MarcUtil.o util.o HtmlParser.o \
           StringUtil.o TextUtil.o Locale.o OCR.o MediaTypeUtil.o MapIO.o MediaTypeUtil.o HtmlParser.o \
           Downloader.o MarcGrepTokenizer.o MarcQueryParser.o SmartDownloader.o BibleReferenceParser.o \
           ExecUtil.o PdfUtil.o
	@echo "Linking $@..."
	@ar cqs $@ $^

PdfUtil.o: PdfUtil.h ExecUtil.h util.h
ExecUtil.o: ExecUtil.h util.h
MapIO.o: MapIO.cc MapIO.h StringUtil.h util.h
MediaTypeUtil.o: MediaTypeUtil.cc MediaTypeUtil.h
Locale.o: Locale.cc Locale.h
util.o: util.cc util.h
HtmlParser.o: HtmlParser.cc HtmlParser.h Compiler.h StringUtil.h util.h
StringUtil.o: StringUtil.cc StringUtil.h Compiler.h
TextUtil.o: TextUtil.cc TextUtil.h Locale.h StringUtil.h Compiler.h HtmlParser.h RegexMatcher.h
Subfields.o: Subfields.cc Subfields.h util.h
RegexMatcher.o: RegexMatcher.cc RegexMatcher.h util.h
Leader.o: Leader.cc Leader.h StringUtil.h
DirectoryEntry.o: DirectoryEntry.cc DirectoryEntry.h StringUtil.h util.h
MarcUtil.o: MarcUtil.cc MarcUtil.h DirectoryEntry.h Leader.h util.h Compiler.h Subfields.h
Downloader.o: Downloader.cc Downloader.h util.h
MarcGrepTokenizer.o: MarcGrepTokenizer.cc MarcGrepTokenizer.h StringUtil.h
MarcQueryParser.o: MarcQueryParser.cc DirectoryEntry.h Leader.h MarcQueryParser.h MarcGrepTokenizer.h \
                   RegexMatcher.h StringUtil.h util.h
OCR.o: OCR.cc OCR.h
SmartDownloader.o: SmartDownloader.cc SmartDownloader.h RegexMatcher.h Downloader.h util.h StringUtil.h
BibleReferenceParser.o: BibleReferenceParser.cc BibleReferenceParser.h StringUtil.h Locale.h util.h

clean:
	rm -f *~ $(PROGS) *.o *.a
