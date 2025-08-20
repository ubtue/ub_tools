ALTER TABLE `vufind`.`tuefind_rss_feeds` 
ADD COLUMN `type` ENUM('news', 'calendar') NOT NULL DEFAULT 'news' AFTER `subsystem_types`,
ADD INDEX `type_index` (`type` ASC) VISIBLE;