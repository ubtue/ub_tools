PROGS=marc_grep jop_grep download_test add_isbns_or_issns_to_articles marc_grep_tokenizer_test db_lookup \
      create_full_text_db add_title_keywords augment_bible_references add_child_refs bib_ref_parser_test \
      bib_ref_to_codes_tool extract_text_from_image_only_pdfs shared_buffer_test delete_ids \
      exec_test krimdok_filter resolve_in_path_test enrich_ddcs control_number_filter stemmer_test \
      enrich_keywords_with_title_words test_find_substring test_chop_into_words test_auto_temp_file \
      update_ixtheo_notations update_full_text_db kcdb_info fix_article_biblio_levels marc_info mysql_test \
      deserialise_php_test notify_users send_email_test urlencode regex_matcher xml_parser_test \
      compress_test populate_in_tuebingen_available map_ddc_and_rvk_to_ixtheo_notations dsv_reader getfilename_test
INSTALL_PROGS=$(filter-out %_test,$(PROGS))
CGI_PROGS=full_text_lookup
CCC=clang++
CCOPTS=-g -std=gnu++11 -Wall -Wextra -Werror -Wunused-parameter -Wshadow -Ilib/include -Ilib/libstemmer/include \
       -pedantic -Wno-vla-extension -Wno-parentheses -march=native -O3 -I/opt/shibboleth/include -I/usr/include/libxml2 -c

ifneq ("$(wildcard /etc/centos-release)","")
  LIBMARIADB=-L/usr/lib64/mysql/ -lmysqlclient
  LIBCURL=-L/opt/shibboleth/lib64/ -lcurl
else # Ubuntu version
  LIBMARIADB=-lmariadbclient
  LIBCURL=-lcurl
endif

.PHONY: all clean cgi_install root_install install

%.o: %.cc
	$(CCC) $(CCOPTS) $<

all: $(PROGS) $(CGI_PROGS)

lib/libmarc.a: $(wildcard lib/src/*.cc) $(wildcard lib/include/*.h)
	$(MAKE) -C lib

marc_grep: marc_grep.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto -lrt

marc_grep_static: marc_grep.o lib/libmarc.a
	$(CCC) --static -o $@ $< -Llib -lmarc -lpcre -lcrypto -lssl
	strip $@

marc_grep.o: marc_grep.cc lib/include/Leader.h lib/include/MarcQueryParser.h lib/include/MarcUtil.h \
             lib/include/RegexMatcher.h lib/include/Subfields.h lib/include/util.h

extract_text_from_image_only_pdfs.o: extract_text_from_image_only_pdfs.cc lib/include/PdfUtil.h \
                                     lib/include/ExecUtil.h lib/include/FileUtil.h

extract_text_from_image_only_pdfs: extract_text_from_image_only_pdfs.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto -lssl

marc_grep_tokenizer_test: marc_grep_tokenizer_test.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

marc_grep_tokenizer_test.o: marc_grep_tokenizer_test.cc lib/include/MarcGrepTokenizer.h

jop_grep: jop_grep.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

jop_grep.o: jop_grep.cc lib/include/MarcUtil.h lib/include/DirectoryEntry.h lib/include/Leader.h \
            lib/include/RegexMatcher.h lib/include/util.h lib/include/StringUtil.h

add_isbns_or_issns_to_articles: add_isbns_or_issns_to_articles.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

add_isbns_or_issns_to_articles.o: add_isbns_or_issns_to_articles.cc lib/include/MarcUtil.h lib/include/DirectoryEntry.h \
                                  lib/include/Leader.h lib/include/RegexMatcher.h lib/include/util.h lib/include/StringUtil.h

add_title_keywords.o: add_title_keywords.cc lib/include/MarcUtil.h lib/include/DirectoryEntry.h lib/include/Leader.h \
                      lib/include/util.h lib/include/StringUtil.h lib/include/Subfields.h lib/include/TextUtil.h

add_title_keywords: add_title_keywords.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

add_child_refs.o: add_child_refs.cc lib/include/MarcUtil.h lib/include/DirectoryEntry.h lib/include/Leader.h \
                  lib/include/util.h lib/include/StringUtil.h lib/include/Subfields.h

add_child_refs: add_child_refs.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

fix_article_biblio_levels.o: fix_article_biblio_levels.cc lib/include/MarcUtil.h lib/include/DirectoryEntry.h \
                             lib/include/Leader.h lib/include/RegexMatcher.h lib/include/StringUtil.h \
                             lib/include/Subfields.h lib/include/util.h

fix_article_biblio_levels: fix_article_biblio_levels.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

populate_in_tuebingen_available.o: populate_in_tuebingen_available.cc lib/include/MarcUtil.h lib/include/DirectoryEntry.h \
                                   lib/include/Leader.h lib/include/StringUtil.h lib/include/Subfields.h lib/include/util.h

populate_in_tuebingen_available: populate_in_tuebingen_available.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -lpcre -lcrypto

augment_bible_references.o: augment_bible_references.cc lib/include/MarcUtil.h lib/include/DirectoryEntry.h lib/include/Leader.h \
                            lib/include/util.h lib/include/StringUtil.h lib/include/TextUtil.h \
                            lib/include/BibleReferenceParser.h lib/include/MapIO.h

augment_bible_references: augment_bible_references.o lib/libmarc.a
	$(CCC) -o $@ augment_bible_references.o -Llib -lmarc -lpcre -lcrypto

bib_ref_parser_test.o: bib_ref_parser_test.cc lib/include/BibleReferenceParser.h lib/include/util.h

bib_ref_parser_test: bib_ref_parser_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

bib_ref_to_codes_tool.o: bib_ref_to_codes_tool.cc lib/include/BibleReferenceParser.h lib/include/StringUtil.h lib/include/util.h \
                         lib/include/MapIO.h lib/include/RegexMatcher.h

bib_ref_to_codes_tool: bib_ref_to_codes_tool.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

download_test.o: download_test.cc lib/include/SmartDownloader.h lib/include/util.h lib/include/FileUtil.h lib/include/TimeLimit.h \
                 lib/include/StringUtil.h

download_test: download_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic -lrt -lssl $(LIBCURL)

create_full_text_db.o: create_full_text_db.cc lib/include/Downloader.h lib/include/RegexMatcher.h lib/include/StringUtil.h \
                       lib/include/util.h lib/include/MarcUtil.h lib/include/Subfields.h lib/include/ExecUtil.h \
                       lib/include/FileLocker.h lib/include/FileUtil.h lib/include/TimeLimit.h lib/include/util.h

create_full_text_db: create_full_text_db.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic

update_full_text_db: update_full_text_db.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lmagic -lrt -lssl $(LIBCURL)

update_full_text_db.o: update_full_text_db.cc lib/include/DirectoryEntry.h lib/include/ExecUtil.h lib/include/FileLocker.h \
                       lib/include/FileUtil.h lib/include/MarcUtil.h lib/include/MediaTypeUtil.h lib/include/PdfUtil.h \
                       lib/include/SmartDownloader.h lib/include/StringUtil.h lib/include/Subfields.h lib/include/util.h

delete_ids.o: delete_ids.cc lib/include/MarcUtil.h lib/include/StringUtil.h lib/include/Subfields.h lib/include/util.h

delete_ids: delete_ids.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lmagic

db_lookup.o: db_lookup.cc lib/include/util.h

db_lookup: db_lookup.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet

full_text_lookup.o: full_text_lookup.cc lib/include/util.h

full_text_lookup: full_text_lookup.o lib/libmarc.a
	$(CCC) -o $@ $^ -L. -lpcre -lcrypto -lkyotocabinet

shared_buffer_test: shared_buffer_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto -lkyotocabinet -lpthread

shared_buffer_test.o: shared_buffer_test.cc lib/include/SharedBuffer.h lib/include/ThreadManager.h lib/include/util.h \
                      lib/include/StringUtil.h

exec_test: exec_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

exec_test.o: exec_test.cc lib/include/ExecUtil.h lib/include/util.h lib/include/StringUtil.h

resolve_in_path_test: resolve_in_path_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

resolve_in_path_test.o: resolve_in_path_test.cc lib/include/ExecUtil.h

krimdok_filter: krimdok_filter.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

krimdok_filter.o: krimdok_filter.cc lib/include/DirectoryEntry.h lib/include/Leader.h lib/include/MarcUtil.h \
                  lib/include/RegexMatcher.h lib/include/StringUtil.h lib/include/util.h

enrich_ddcs.o: enrich_ddcs.cc lib/include/DirectoryEntry.h lib/include/Leader.h lib/include/MarcUtil.h lib/include/RegexMatcher.h \
               lib/include/Subfields.h lib/include/util.h

enrich_ddcs: enrich_ddcs.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

control_number_filter.o: control_number_filter.cc lib/include/DirectoryEntry.h lib/include/Leader.h lib/include/MarcUtil.h \
                         lib/include/RegexMatcher.h lib/include/util.h

control_number_filter: control_number_filter.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto


stemmer_test.o: stemmer_test.cc lib/include/Stemmer.h lib/include/util.h

stemmer_test: stemmer_test.o lib/libmarc.a lib/libstemmer/libstemmer.a
	$(CCC) -o $@ $^ -Llib -lmarc -Llib/libstemmer -lstemmer -lpcre -lcrypto

enrich_keywords_with_title_words.o: enrich_keywords_with_title_words.cc lib/include/MarcUtil.h lib/include/DirectoryEntry.h \
                                    lib/include/Leader.h lib/include/util.h lib/include/StringUtil.h lib/include/TextUtil.h \
                                    lib/include/Subfields.h lib/include/Stemmer.h lib/include/RegexMatcher.h

enrich_keywords_with_title_words: enrich_keywords_with_title_words.o lib/libmarc.a
	$(CCC) -o $@ $< -Llib -lmarc -Llib/libstemmer -lstemmer -lpcre -lcrypto

update_ixtheo_notations.o: update_ixtheo_notations.cc lib/include/DirectoryEntry.h lib/include/Leader.h lib/include/MarcUtil.h \
                           lib/include/Subfields.h lib/include/util.h

update_ixtheo_notations: update_ixtheo_notations.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

map_ddc_and_rvk_to_ixtheo_notations.o: map_ddc_and_rvk_to_ixtheo_notations.cc lib/include/MarcUtil.h lib/include/StringUtil.h \
                                       lib/include/Subfields.h lib/include/util.h

map_ddc_and_rvk_to_ixtheo_notations: map_ddc_and_rvk_to_ixtheo_notations.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

test_find_substring.o: test_find_substring.cc lib/include/TextUtil.h lib/include/StringUtil.h lib/include/util.h

test_find_substring: test_find_substring.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

test_chop_into_words.o: test_chop_into_words.cc lib/include/TextUtil.h lib/include/util.h

test_chop_into_words: test_chop_into_words.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

kcdb_info.o: kcdb_info.cc lib/include/util.h

kcdb_info: kcdb_info.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lkyotocabinet

test_auto_temp_file.o: test_auto_temp_file.cc lib/include/FileUtil.h

test_auto_temp_file: test_auto_temp_file.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lpcre -lcrypto

marc_info.o: marc_info.cc lib/include/Leader.h lib/include/MarcUtil.h lib/include/util.h

marc_info: marc_info.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto

mysql_test.o: mysql_test.cc lib/include/DbConnection.h lib/include/DbResultSet.h lib/include/DbRow.h lib/include/util.h

mysql_test: mysql_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -lcrypto $(LIBMARIADB) -Llib -lmarc

regex_matcher.o: regex_matcher.cc lib/include/RegexMatcher.h lib/include/util.h

regex_matcher: regex_matcher.o lib/libmarc.a
	$(CCC) -o $@ $^ -lcrypto $(LIBMARIADB) -Llib -lmarc -lpcre

dsv_reader.o: dsv_reader.cc lib/include/util.h

dsv_reader: dsv_reader.o lib/libmarc.a
	$(CCC) -o $@ $^ -lcrypto $(LIBMARIADB) -Llib -lmarc -lpcre

notify_users.o: notify_users.cc lib/include/DbConnection.h lib/include/DbResultSet.h lib/include/DbRow.h \
                lib/include/util.h lib/include/Compiler.h lib/include/UrlUtil.h lib/include/Downloader.h \
                lib/include/StringUtil.h lib/include/RegexMatcher.h lib/include/XmlParser.h \
		lib/include/GzStream.h

notify_users: notify_users.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto $(LIBMARIADB) -lpcre -lxml2 -lz -lmagic -lrt -lssl $(LIBCURL)

xml_parser_test.o: xml_parser_test.cc lib/include/util.h lib/include/XmlParser.h lib/include/StringUtil.h

xml_parser_test: xml_parser_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto $(LIBMARIADB) -lpcre -lxml2

getfilename_test.o: getfilename_test.cc lib/include/FileUtil.h lib/include/util.h

getfilename_test: getfilename_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto $(LIBMARIADB) -lpcre -lxml2

deserialise_php_test.o: deserialise_php_test.cc lib/include/util.h lib/include/PHPUtil.h lib/include/Compiler.h

deserialise_php_test: deserialise_php_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto

send_email_test.o: send_email_test.cc lib/include/util.h lib/include/EmailSender.h

send_email_test: send_email_test.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto

urlencode.o: urlencode.cc lib/include/util.h lib/include/UrlUtil.h

urlencode: urlencode.o lib/libmarc.a
	$(CCC) -o $@ $^ -Llib -lmarc -lcrypto -lpcre -lmagic -lrt -lssl $(LIBCURL)

compress_test.o: compress_test.cc lib/include/util.h lib/include/GzStream.h

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
