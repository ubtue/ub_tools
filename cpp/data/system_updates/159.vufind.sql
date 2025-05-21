ALTER TABLE `vufind`.`tuefind_rss_feeds` 
ADD COLUMN `type` SET('feed', 'news', 'calendar') NOT NULL DEFAULT 'news' AFTER `subsystem_types`;