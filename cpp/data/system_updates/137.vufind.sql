ALTER TABLE tuefind_publications ADD doi VARCHAR(255) NULL AFTER external_document_guid;
ALTER TABLE tuefind_publications ADD doi_notification DATETIME NULL AFTER doi;