ALTER TABLE tuefind_publications ADD doi_link VARCHAR(255) NULL AFTER external_document_guid;
ALTER TABLE tuefind_publications ADD doi_notification DATE NULL AFTER doi_link;