ALTER TABLE `keyword_translations` ADD INDEX keyword_translations_idx_prev_version_id (prev_version_id);
ALTER TABLE `keyword_translations` ADD INDEX keyword_translations_idx_next_version_id (next_version_id);
ALTER TABLE `keyword_translations` ADD INDEX keyword_translations_idx_origin (origin);
