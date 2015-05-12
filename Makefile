PROGS=marc_grep2 jop_grep download_test add_issn_to_articles marc_grep_tokenizer_test db_lookup \
      create_full_text_db add_title_keywords augment_bible_references add_child_refs bib_ref_parser_test \
      bib_ref_to_codes_tool
CCC=g++
CCOPTS=-g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -O3 -c
LDOPTS=-O3

%.o: %.cc
	$(CCC) $(CCOPTS) $<


all: $(PROGS)

marc_grep2: marc_grep2.o MarcGrepTokenizer.o MarcQueryParser.o libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< MarcGrepTokenizer.o MarcQueryParser.o -L. -lmarc -lpcre -lcrypto

marc_grep2.o: marc_grep2.cc Leader.h MarcQueryParser.h MarcUtil.h RegexMatcher.h Subfields.h util.h
	$(CCC) $(CCOPTS) $<

marc_grep_tokenizer_test: marc_grep_tokenizer_test.o MarcGrepTokenizer.o libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< MarcGrepTokenizer.o -L. -lmarc -lpcre -lcrypto

marc_grep_tokenizer_test.o: marc_grep_tokenizer_test.cc MarcGrepTokenizer.h
	$(CCC) $(CCOPTS) $<

jop_grep: jop_grep.o libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -L. -lmarc -lpcre -lcrypto

jop_grep.o: jop_grep.cc MarcUtil.h DirectoryEntry.h Leader.h RegexMatcher.h util.h StringUtil.h
	$(CCC) $(CCOPTS) $<

add_issn_to_articles: add_issn_to_articles.o libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -L. -lmarc -lpcre -lcrypto

add_issn_to_articles.o: add_issn_to_articles.cc MarcUtil.h DirectoryEntry.h Leader.h RegexMatcher.h util.h \
                        StringUtil.h
	$(CCC) $(CCOPTS) $<

add_title_keywords.o: add_title_keywords.cc MarcUtil.h DirectoryEntry.h Leader.h util.h StringUtil.h \
                      TextUtil.h
	$(CCC) $(CCOPTS) $<

add_title_keywords: add_title_keywords.o libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -L. -lmarc -lpcre -lcrypto

add_child_refs.o: add_child_refs.cc MarcUtil.h DirectoryEntry.h Leader.h util.h StringUtil.h Subfields.h
	$(CCC) $(CCOPTS) $<

add_child_refs: add_child_refs.o libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -L. -lmarc -lpcre -lcrypto

augment_bible_references.o: augment_bible_references.cc MarcUtil.h DirectoryEntry.h Leader.h util.h StringUtil.h \
                            TextUtil.h BibleReferenceParser.h MapIO.h
	$(CCC) $(CCOPTS) $<

augment_bible_references: augment_bible_references.o  BibleReferenceParser.o libmarc.a
	$(CCC) $(LDFLAGS) -o $@ augment_bible_references.o  BibleReferenceParser.o -L. -lmarc -lpcre -lcrypto

libmarc.a: Subfields.o RegexMatcher.o Leader.o StringUtil.o DirectoryEntry.o MarcUtil.o util.o HtmlParser.o \
           StringUtil.o TextUtil.o Locale.o OCR.o MediaTypeUtil.o MapIO.o
	@echo "Linking $@..."
	@ar cqs $@ $^

MapIO.o: MapIO.cc MapIO.h StringUtil.h util.h
	$(CCC) $(CCOPTS) $<

MediaTypeUtil.o: MediaTypeUtil.cc MediaTypeUtil.h
	$(CCC) $(CCOPTS) $<

Locale.o: Locale.cc Locale.h
	$(CCC) $(CCOPTS) $<

util.o: util.cc util.h
	$(CCC) $(CCOPTS) $<

HtmlParser.o: HtmlParser.cc HtmlParser.h Compiler.h StringUtil.h util.h
	$(CCC) $(CCOPTS) $<

StringUtil.o: StringUtil.cc StringUtil.h Compiler.h
	$(CCC) $(CCOPTS) $<

TextUtil.o: TextUtil.cc TextUtil.h Locale.h StringUtil.h Compiler.h HtmlParser.h RegexMatcher.h
	$(CCC) $(CCOPTS) $<

Subfields.o: Subfields.cc Subfields.h util.h
	$(CCC) $(CCOPTS) $<

RegexMatcher.o: RegexMatcher.cc RegexMatcher.h util.h
	$(CCC) $(CCOPTS) $<

Leader.o: Leader.cc Leader.h StringUtil.h
	$(CCC) $(CCOPTS) $<

DirectoryEntry.o: DirectoryEntry.cc DirectoryEntry.h StringUtil.h util.h
	$(CCC) $(CCOPTS) $<

MarcUtil.o: MarcUtil.cc MarcUtil.h DirectoryEntry.h Leader.h util.h Compiler.h Subfields.h
	$(CCC) $(CCOPTS) $<

Downloader.o: Downloader.cc Downloader.h util.h
	$(CCC) $(CCOPTS) $<

MarcGrepTokenizer.o: MarcGrepTokenizer.cc MarcGrepTokenizer.h StringUtil.h
	$(CCC) $(CCOPTS) $<

MarcQueryParser.o: MarcQueryParser.cc DirectoryEntry.h Leader.h MarcQueryParser.h MarcGrepTokenizer.h \
                   RegexMatcher.h StringUtil.h util.h
	$(CCC) $(CCOPTS) $<

OCR.o: OCR.cc OCR.h
	$(CCC) $(CCOPTS) $<

SmartDownloader.o: SmartDownloader.cc SmartDownloader.h RegexMatcher.h Downloader.h util.h StringUtil.h
	$(CCC) $(CCOPTS) $<

BibleReferenceParser.o: BibleReferenceParser.cc BibleReferenceParser.h StringUtil.h Locale.h util.h
	$(CCC) $(CCOPTS) $<

bib_ref_parser_test.o: bib_ref_parser_test.cc BibleReferenceParser.h util.h
	$(CCC) $(CCOPTS) $<

bib_ref_parser_test: bib_ref_parser_test.o BibleReferenceParser.o
	$(CCC) $(LDFLAGS) -o $@ $^ -L. -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic

bib_ref_to_codes_tool.o: bib_ref_to_codes_tool.cc BibleReferenceParser.h StringUtil.h util.h MapIO.h
	$(CCC) $(CCOPTS) $<

bib_ref_to_codes_tool: bib_ref_to_codes_tool.o BibleReferenceParser.o
	$(CCC) $(LDFLAGS) -o $@ $^ -L. -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic

download_test.o: download_test.cc Downloader.h RegexMatcher.h StringUtil.h TextUtil.h util.h MediaTypeUtil.h
	$(CCC) $(CCOPTS) $<

download_test: download_test.o Downloader.o libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -L. -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic

create_full_text_db.o: create_full_text_db.cc Downloader.h RegexMatcher.h StringUtil.h TextUtil.h util.h \
                       MediaTypeUtil.h MarcUtil.h Subfields.h SharedBuffer.h SmartDownloader.h
	$(CCC) $(CCOPTS) $<

create_full_text_db: create_full_text_db.o Downloader.o libmarc.a SmartDownloader.o
	$(CCC) $(LDFLAGS) -o $@ $^ -L. -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic

db_lookup.o: db_lookup.cc util.h
	$(CCC) $(CCOPTS) $<

db_lookup: db_lookup.o libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -L. -lmarc -lpcre -lcrypto -lkyotocabinet

clean:
	rm -f *~ $(PROGS) *.o *.a

backup:
	cp *.cc *.h *.sh *.js Makefile LICENSE COPYING stopwords.??? /mnt/krimdok/source/marc_filter/
	cp *.cc *.h *.sh *.js Makefile LICENSE COPYING stopwords.??? ~/Google\ Drive/marc_grep/
