ALTER TABLE keyword_translations MODIFY COLUMN status enum ('reliable_synonym','reliable','unreliable', 'unreliable_cat2', 'unreliable_synonym','replaced','replaced_synonym','new','new_synonym');
ALTER TABLE keyword_translations ADD COLUMN wikidata_id char(12) AFTER gnd_code;
ALTER TABLE `keyword_translations` ADD INDEX keyword_translations_wikidata_id (wikidata_id);
