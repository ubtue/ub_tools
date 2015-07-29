PROGS=marc_grep jop_grep download_test add_isbns_or_issns_to_articles marc_grep_tokenizer_test db_lookup \
      create_full_text_db add_title_keywords augment_bible_references add_child_refs bib_ref_parser_test \
      bib_ref_to_codes_tool extract_text_from_image_only_pdfs shared_buffer_test delete_ids marc_filter \
      exec_test krimdok_filter resolve_in_path_test enrich_ddcs
INSTALL_PROGS=$(filter-out %_test,$(PROGS))
CGI_PROGS=full_text_lookup
CCC=g++
CCOPTS=-g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Ilib -O3 -march=native -c
LDOPTS=-O3

.PHONY: clean

%.o: %.cc
	$(CCC) $(CCOPTS) $<

all: $(PROGS) $(CGI_PROGS)

lib/libmarc.a: $(wildcard lib/*.cc) $(wildcard lib/*.h)
	$(MAKE) -C lib

marc_grep: marc_grep.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

marc_grep.o: marc_grep.cc lib/Leader.h lib/MarcQueryParser.h lib/MarcUtil.h lib/RegexMatcher.h lib/Subfields.h \
              lib/util.h

extract_text_from_image_only_pdfs.o: extract_text_from_image_only_pdfs.cc lib/PdfUtil.h lib/ExecUtil.h lib/FileUtil.h

extract_text_from_image_only_pdfs: extract_text_from_image_only_pdfs.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

marc_grep_tokenizer_test: marc_grep_tokenizer_test.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

marc_grep_tokenizer_test.o: marc_grep_tokenizer_test.cc lib/MarcGrepTokenizer.h

jop_grep: jop_grep.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

jop_grep.o: jop_grep.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h lib/RegexMatcher.h lib/util.h \
            lib/StringUtil.h

add_isbns_or_issns_to_articles: add_isbns_or_issns_to_articles.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

add_isbns_or_issns_to_articles.o: add_isbns_or_issns_to_articles.cc lib/MarcUtil.h lib/DirectoryEntry.h \
                                  lib/Leader.h lib/RegexMatcher.h lib/util.h lib/StringUtil.h

add_title_keywords.o: add_title_keywords.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h lib/util.h \
                      lib/StringUtil.h lib/TextUtil.h

add_title_keywords: add_title_keywords.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

add_child_refs.o: add_child_refs.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h lib/util.h lib/StringUtil.h \
                  lib/Subfields.h

add_child_refs: add_child_refs.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $< -Llib -lmarc -lpcre -lcrypto

augment_bible_references.o: augment_bible_references.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h \
                            lib/util.h lib/StringUtil.h lib/TextUtil.h lib/BibleReferenceParser.h lib/MapIO.h

augment_bible_references: augment_bible_references.o  lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ augment_bible_references.o -Llib -lmarc -lpcre -lcrypto

bib_ref_parser_test.o: bib_ref_parser_test.cc lib/BibleReferenceParser.h lib/util.h

bib_ref_parser_test: bib_ref_parser_test.o
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

bib_ref_to_codes_tool.o: bib_ref_to_codes_tool.cc lib/BibleReferenceParser.h lib/StringUtil.h lib/util.h \
                         lib/MapIO.h lib/RegexMatcher.h

bib_ref_to_codes_tool: bib_ref_to_codes_tool.o
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

download_test.o: download_test.cc lib/SmartDownloader.h lib/util.h lib/FileUtil.h lib/TimeLimit.h \
                 lib/StringUtil.h

download_test: download_test.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic

create_full_text_db.o: create_full_text_db.cc lib/Downloader.h lib/RegexMatcher.h lib/StringUtil.h \
                       lib/TextUtil.h lib/util.h lib/MediaTypeUtil.h lib/MarcUtil.h lib/Subfields.h \
                       lib/SharedBuffer.h lib/SmartDownloader.h lib/PdfUtil.h lib/ExecUtil.h lib/FileUtil.h \
	               lib/TimeLimit.h lib/util.h lib/ThreadManager.h lib/ThreadSafeCounter.h

create_full_text_db: create_full_text_db.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic -lpthread

delete_ids.o: delete_ids.cc lib/MarcUtil.h lib/StringUtil.h lib/Subfields.h lib/util.h

delete_ids: delete_ids.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

db_lookup.o: db_lookup.cc lib/util.h

db_lookup: db_lookup.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet

full_text_lookup.o: full_text_lookup.cc lib/util.h

full_text_lookup: full_text_lookup.o lib/libmarc.a
	$(CCC) -o $@ $^ -L. -lpcre -lcrypto -lkyotocabinet

shared_buffer_test: shared_buffer_test.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lpthread

shared_buffer_test.o: shared_buffer_test.cc lib/SharedBuffer.h lib/ThreadManager.h lib/util.h lib/StringUtil.h

marc_filter: marc_filter.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

marc_filter.o: marc_filter.cc lib/DirectoryEntry.h lib/Leader.h lib/MarcUtil.h lib/RegexMatcher.h \
               lib/StringUtil.h lib/util.h

exec_test: exec_test.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

exec_test.o: exec_test.cc lib/ExecUtil.h lib/util.h lib/StringUtil.h

resolve_in_path_test: resolve_in_path_test.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

resolve_in_path_test.o: resolve_in_path_test.cc lib/ExecUtil.h

krimdok_filter: krimdok_filter.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

krimdok_filter.o: krimdok_filter.cc lib/DirectoryEntry.h lib/Leader.h lib/MarcUtil.h lib/RegexMatcher.h \
                  lib/StringUtil.h lib/util.h

enrich_ddcs.o: enrich_ddcs.cc lib/DirectoryEntry.h lib/Leader.h lib/MarcUtil.h lib/RegexMatcher.h \
               lib/Subfields.h lib/util.h

enrich_ddcs: enrich_ddcs.o lib/libmarc.a
	$(CCC) $(LDFLAGS) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

clean:
	rm -f *~ $(PROGS) *.o

cgi_install: $(CGI_PROGS)
	cp full_text_lookup /var/www/cgi-bin/

root_install: $(INSTALL_PROGS) *.sh
	cp $^ /usr/local/bin/

install: $(INSTALL_PROGS) *.sh
	cp $^ ~/bin/
