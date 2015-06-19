PROGS=marc_grep2 jop_grep download_test add_issn_to_articles marc_grep_tokenizer_test db_lookup \
      create_full_text_db add_title_keywords augment_bible_references add_child_refs bib_ref_parser_test \
      bib_ref_to_codes_tool extract_text_from_image_only_pdfs shared_buffer_test delete_ids
CCC=g++
CCOPTS=-g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Ilib -O3 -c
LDOPTS=-O3

.PHONY: clean

%.o: %.cc
	$(CCC) $(CCOPTS) $<

all: $(PROGS)

lib/libmarc.a: $(wildcard lib/*.cc) $(wildcard lib/*.h)
	$(MAKE) -C lib

marc_grep2: marc_grep2.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

marc_grep2.o: marc_grep2.cc lib/Leader.h lib/MarcQueryParser.h lib/MarcUtil.h lib/RegexMatcher.h lib/Subfields.h \
              lib/util.h
	$(CCC) $(CCOPTS) $<

extract_text_from_image_only_pdfs.o: extract_text_from_image_only_pdfs.cc lib/PdfUtil.h lib/ExecUtil.h lib/FileUtil.h
	$(CCC) $(CCOPTS) $<

extract_text_from_image_only_pdfs: extract_text_from_image_only_pdfs.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

marc_grep_tokenizer_test: marc_grep_tokenizer_test.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

marc_grep_tokenizer_test.o: marc_grep_tokenizer_test.cc lib/MarcGrepTokenizer.h
	$(CCC) $(CCOPTS) $<

jop_grep: jop_grep.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

jop_grep.o: jop_grep.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h lib/RegexMatcher.h lib/util.h \
            lib/StringUtil.h
	$(CCC) $(CCOPTS) $<

add_issn_to_articles: add_issn_to_articles.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

add_issn_to_articles.o: add_issn_to_articles.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h lib/RegexMatcher.h \
                        lib/util.h lib/StringUtil.h
	$(CCC) $(CCOPTS) $<

add_title_keywords.o: add_title_keywords.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h lib/util.h \
                      lib/StringUtil.h lib/TextUtil.h
	$(CCC) $(CCOPTS) $<

add_title_keywords: add_title_keywords.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

add_child_refs.o: add_child_refs.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h lib/util.h lib/StringUtil.h \
                  lib/Subfields.h
	$(CCC) $(CCOPTS) $<

add_child_refs: add_child_refs.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

augment_bible_references.o: augment_bible_references.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h \
                            lib/util.h lib/StringUtil.h lib/TextUtil.h lib/BibleReferenceParser.h lib/MapIO.h
	$(CCC) $(CCOPTS) $<

augment_bible_references: augment_bible_references.o  lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ augment_bible_references.o -Llib -lmarc -lpcre -lcrypto

bib_ref_parser_test.o: bib_ref_parser_test.cc lib/BibleReferenceParser.h lib/util.h
	$(CCC) $(CCOPTS) $<

bib_ref_parser_test: bib_ref_parser_test.o
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

bib_ref_to_codes_tool.o: bib_ref_to_codes_tool.cc lib/BibleReferenceParser.h lib/StringUtil.h lib/util.h \
                         lib/MapIO.h lib/RegexMatcher.h
	$(CCC) $(CCOPTS) $<

bib_ref_to_codes_tool: bib_ref_to_codes_tool.o
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

download_test.o: download_test.cc lib/SmartDownloader.h lib/util.h lib/FileUtil.h lib/TimeLimit.h \
                 lib/StringUtil.h
	$(CCC) $(CCOPTS) $<

download_test: download_test.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic

create_full_text_db.o: create_full_text_db.cc lib/Downloader.h lib/RegexMatcher.h lib/StringUtil.h \
                       lib/TextUtil.h lib/util.h lib/MediaTypeUtil.h lib/MarcUtil.h lib/Subfields.h \
                       lib/SharedBuffer.h lib/SmartDownloader.h lib/PdfUtil.h lib/ExecUtil.h lib/FileUtil.h \
	               lib/TimeLimit.h lib/util.h lib/ThreadManager.h lib/ThreadSafeCounter.h
	$(CCC) $(CCOPTS) $<

create_full_text_db: create_full_text_db.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic -lpthread

delete_ids.o: delete_ids.cc lib/MarcUtil.h  lib/util.h
	$(CCC) $(CCOPTS) $<

delete_ids: delete_ids.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

db_lookup.o: db_lookup.cc lib/util.h
	$(CCC) $(CCOPTS) $<

db_lookup: db_lookup.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet

shared_buffer_test: shared_buffer_test.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lpthread

shared_buffer_test.o: shared_buffer_test.cc lib/SharedBuffer.h lib/ThreadManager.h lib/util.h lib/StringUtil.h
	$(CCC) $(CCOPTS) $<

clean:
	rm -f *~ $(PROGS) *.o
