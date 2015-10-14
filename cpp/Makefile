PROGS=marc_grep jop_grep download_test add_isbns_or_issns_to_articles marc_grep_tokenizer_test db_lookup \
      create_full_text_db add_title_keywords augment_bible_references add_child_refs bib_ref_parser_test \
      bib_ref_to_codes_tool extract_text_from_image_only_pdfs shared_buffer_test delete_ids \
      exec_test krimdok_filter resolve_in_path_test enrich_ddcs control_number_filter stemmer_test \
      enrich_keywords_with_title_words test_find_substring test_chop_into_words test_auto_temp_file \
      update_ixtheo_notations update_full_text_db kcdb_info fix_article_biblio_levels marc_info mysql_test \
      deserialise_php_test notify_users send_email_test urlencode regex_matcher xml_parser_test \
      compress_test populate_in_tuebingen_available
INSTALL_PROGS=$(filter-out %_test,$(PROGS))
CGI_PROGS=full_text_lookup
CCC=g++
CCOPTS=-g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -Ilib -Ilib/libstemmer/include \
       -pedantic -march=native -O3 -I/usr/include/libxml2 -c

ifneq ("$(wildcard /etc/centos-release)","")
  LIBMARIADB=-L/usr/lib64/mysql/ -lmysqlclient
else # Ubuntu version
  LIBMARIADB=-lmariadbclient
endif

.PHONY: all clean cgi_install root_install install

%.o: %.cc
	$(CCC) $(CCOPTS) $<

all: $(PROGS) $(CGI_PROGS)

lib/libmarc.a: $(wildcard lib/*.cc) $(wildcard lib/*.h)
	$(MAKE) -C lib

marc_grep: marc_grep.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

marc_grep_static: marc_grep.o lib/libmarc.a
	$(CCC) --static -o $@ $< -Llib -lmarc -lpcre -lcrypto
	strip $@

marc_grep.o: marc_grep.cc lib/Leader.h lib/MarcQueryParser.h lib/MarcUtil.h lib/RegexMatcher.h lib/Subfields.h \
              lib/util.h

extract_text_from_image_only_pdfs.o: extract_text_from_image_only_pdfs.cc lib/PdfUtil.h lib/ExecUtil.h lib/FileUtil.h

extract_text_from_image_only_pdfs: extract_text_from_image_only_pdfs.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

marc_grep_tokenizer_test: marc_grep_tokenizer_test.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

marc_grep_tokenizer_test.o: marc_grep_tokenizer_test.cc lib/MarcGrepTokenizer.h

jop_grep: jop_grep.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

jop_grep.o: jop_grep.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h lib/RegexMatcher.h lib/util.h \
            lib/StringUtil.h

add_isbns_or_issns_to_articles: add_isbns_or_issns_to_articles.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

add_isbns_or_issns_to_articles.o: add_isbns_or_issns_to_articles.cc lib/MarcUtil.h lib/DirectoryEntry.h \
                                  lib/Leader.h lib/RegexMatcher.h lib/util.h lib/StringUtil.h

add_title_keywords.o: add_title_keywords.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h lib/util.h \
                      lib/StringUtil.h lib/Subfields.h lib/TextUtil.h

add_title_keywords: add_title_keywords.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

add_child_refs.o: add_child_refs.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h lib/util.h lib/StringUtil.h \
                  lib/Subfields.h

add_child_refs: add_child_refs.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

fix_article_biblio_levels.o: fix_article_biblio_levels.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h \
                             lib/RegexMatcher.h lib/StringUtil.h lib/Subfields.h lib/util.h

fix_article_biblio_levels: fix_article_biblio_levels.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

populate_in_tuebingen_available.o: populate_in_tuebingen_available.cc lib/MarcUtil.h lib/DirectoryEntry.h \
                                   lib/Leader.h lib/StringUtil.h lib/Subfields.h lib/util.h

populate_in_tuebingen_available: populate_in_tuebingen_available.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

augment_bible_references.o: augment_bible_references.cc lib/MarcUtil.h lib/DirectoryEntry.h lib/Leader.h \
                            lib/util.h lib/StringUtil.h lib/TextUtil.h lib/BibleReferenceParser.h lib/MapIO.h

augment_bible_references: augment_bible_references.o  lib/libmarc.a
	$(CCC) -o $@ augment_bible_references.o -Llib -lmarc -lpcre -lcrypto

bib_ref_parser_test.o: bib_ref_parser_test.cc lib/BibleReferenceParser.h lib/util.h

bib_ref_parser_test: bib_ref_parser_test.o
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

bib_ref_to_codes_tool.o: bib_ref_to_codes_tool.cc lib/BibleReferenceParser.h lib/StringUtil.h lib/util.h \
                         lib/MapIO.h lib/RegexMatcher.h

bib_ref_to_codes_tool: bib_ref_to_codes_tool.o
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

download_test.o: download_test.cc lib/SmartDownloader.h lib/util.h lib/FileUtil.h lib/TimeLimit.h \
                 lib/StringUtil.h

download_test: download_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic

create_full_text_db.o: create_full_text_db.cc lib/Downloader.h lib/RegexMatcher.h lib/StringUtil.h lib/util.h \
                       lib/MarcUtil.h lib/Subfields.h lib/ExecUtil.h lib/FileLocker.h lib/FileUtil.h \
                       lib/TimeLimit.h lib/util.h

create_full_text_db: create_full_text_db.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic

update_full_text_db: update_full_text_db.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic

update_full_text_db.o: update_full_text_db.cc lib/DirectoryEntry.h lib/ExecUtil.h lib/FileLocker.h lib/FileUtil.h \
                       lib/MarcUtil.h lib/MediaTypeUtil.h lib/PdfUtil.h lib/SmartDownloader.h lib/StringUtil.h \
                       lib/Subfields.h lib/util.h

delete_ids.o: delete_ids.cc lib/MarcUtil.h lib/StringUtil.h lib/Subfields.h lib/util.h

delete_ids: delete_ids.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

db_lookup.o: db_lookup.cc lib/util.h

db_lookup: db_lookup.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet

full_text_lookup.o: full_text_lookup.cc lib/util.h

full_text_lookup: full_text_lookup.o lib/libmarc.a
	$(CCC) -o $@ $^ -L. -lpcre -lcrypto -lkyotocabinet

shared_buffer_test: shared_buffer_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lpthread

shared_buffer_test.o: shared_buffer_test.cc lib/SharedBuffer.h lib/ThreadManager.h lib/util.h lib/StringUtil.h

exec_test: exec_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

exec_test.o: exec_test.cc lib/ExecUtil.h lib/util.h lib/StringUtil.h

resolve_in_path_test: resolve_in_path_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

resolve_in_path_test.o: resolve_in_path_test.cc lib/ExecUtil.h

krimdok_filter: krimdok_filter.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

krimdok_filter.o: krimdok_filter.cc lib/DirectoryEntry.h lib/Leader.h lib/MarcUtil.h lib/RegexMatcher.h \
                  lib/StringUtil.h lib/util.h

enrich_ddcs.o: enrich_ddcs.cc lib/DirectoryEntry.h lib/Leader.h lib/MarcUtil.h lib/RegexMatcher.h \
               lib/Subfields.h lib/util.h

enrich_ddcs: enrich_ddcs.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

control_number_filter.o: control_number_filter.cc lib/DirectoryEntry.h lib/Leader.h lib/MarcUtil.h \
                         lib/RegexMatcher.h lib/util.h

control_number_filter: control_number_filter.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto


stemmer_test.o: stemmer_test.cc lib/Stemmer.h lib/util.h

stemmer_test: stemmer_test.o lib/libmarc.a lib/libstemmer/libstemmer.a
	$(CCC) -o $@ $^ -Llib -lmarc -Llib/libstemmer -lstemmer -lpcre -lcrypto

enrich_keywords_with_title_words.o: enrich_keywords_with_title_words.cc lib/MarcUtil.h lib/DirectoryEntry.h \
                                    lib/Leader.h lib/util.h lib/StringUtil.h lib/TextUtil.h \
                                    lib/Subfields.h lib/Stemmer.h lib/RegexMatcher.h

enrich_keywords_with_title_words: enrich_keywords_with_title_words.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -Llib/libstemmer -lstemmer -lpcre -lcrypto

update_ixtheo_notations.o: update_ixtheo_notations.cc lib/DirectoryEntry.h lib/Leader.h lib/MarcUtil.h \
                         lib/Subfields.h lib/util.h

update_ixtheo_notations: update_ixtheo_notations.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

test_find_substring.o: test_find_substring.cc lib/TextUtil.h lib/StringUtil.h lib/util.h

test_find_substring: test_find_substring.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

test_chop_into_words.o: test_chop_into_words.cc lib/TextUtil.h lib/util.h

test_chop_into_words: test_chop_into_words.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

kcdb_info.o: kcdb_info.cc lib/util.h

kcdb_info: kcdb_info.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lkyotocabinet

test_auto_temp_file.o: test_auto_temp_file.cc lib/FileUtil.h

test_auto_temp_file: test_auto_temp_file.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

marc_info.o: marc_info.cc lib/Leader.h lib/MarcUtil.h lib/util.h

marc_info: marc_info.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto

mysql_test.o: mysql_test.cc lib/DbConnection.h lib/DbResultSet.h lib/DbRow.h lib/util.h

mysql_test: mysql_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -lcrypto $(LIBMARIADB) -Llib -lmarc

regex_matcher.o: regex_matcher.cc lib/RegexMatcher.h lib/util.h

regex_matcher: regex_matcher.o lib/libmarc.a
	$(CCC) -o $@ $^ -lcrypto $(LIBMARIADB) -Llib -lmarc -lpcre

notify_users.o: notify_users.cc lib/DbConnection.h lib/DbResultSet.h lib/DbRow.h lib/util.h lib/Compiler.h \
                lib/UrlUtil.h lib/Downloader.h lib/StringUtil.h lib/RegexMatcher.h lib/XmlParser.h \
		lib/GzStream.h

notify_users: notify_users.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto $(LIBMARIADB) -lpcre -lxml2 -lz

xml_parser_test.o: xml_parser_test.cc lib/util.h lib/XmlParser.h lib/StringUtil.h

xml_parser_test: xml_parser_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto $(LIBMARIADB) -lpcre -lxml2

deserialise_php_test.o: deserialise_php_test.cc lib/util.h lib/PHPUtil.h lib/Compiler.h

deserialise_php_test: deserialise_php_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto

send_email_test.o: send_email_test.cc lib/util.h lib/EmailSender.h

send_email_test: send_email_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto

urlencode.o: urlencode.cc lib/util.h lib/UrlUtil.h

urlencode: urlencode.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc

compress_test.o: compress_test.cc lib/util.h lib/GzStream.h

compress_test: compress_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lz

clean:
	rm -f *~ $(PROGS) *.o
	cd lib && make clean

cgi_install: $(CGI_PROGS)
	mkdir --parents /var/www/cgi-bin
	cp full_text_lookup /var/www/cgi-bin/

root_install: $(INSTALL_PROGS) *.sh
	cp $^ /usr/local/bin/

install: $(INSTALL_PROGS) *.sh
	cp $^ ~/bin/
