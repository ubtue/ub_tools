ALTER TABLE tuefind_publications ADD COLUMN publication_datetime TIMESTAMP DEFAULT NOW() NOT NULL AFTER terms_date;
