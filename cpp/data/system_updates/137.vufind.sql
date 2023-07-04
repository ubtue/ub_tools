ALTER TABLE tuefind_publications ADD doi VARCHAR(255) DEFAULT NULL AFTER external_document_guid;
ALTER TABLE tuefind_publications ADD doi_notification DATETIME DEFAULT NULL AFTER doi;
CREATE UNIQUE INDEX `publication_doi` ON tuefind_publications (`doi`);
