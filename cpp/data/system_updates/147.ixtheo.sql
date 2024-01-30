ALTER TABLE keyword_translations ADD COLUMN translation_disabled TINYINT(1) AFTER priority_entry;
ALTER TABLE `keyword_translations` ADD INDEX keyword_translations_idx_translation_disabled (translation_disabled);
