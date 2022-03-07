ALTER TABLE metadata_presence_tracer MODIFY COLUMN  record_type ENUM('regular_article', 'review', 'non_article') DEFAULT 'regular_article' NOT NULL
