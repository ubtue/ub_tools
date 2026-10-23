ALTER TABLE `tuefind_cms_pages_history` MODIFY COLUMN `cms_id` INT UNSIGNED DEFAULT NULL;

ALTER TABLE `tuefind_cms_pages_history` ADD COLUMN `subsystem_id` INT UNSIGNED DEFAULT NULL AFTER `cms_id`;
ALTER TABLE `tuefind_cms_pages_history` ADD KEY `subsystem_id` (`subsystem_id`);
ALTER TABLE `tuefind_cms_pages_history` ADD CONSTRAINT `fk_history_subsystem` FOREIGN KEY (`subsystem_id`) REFERENCES `tuefind_subsystems` (`id`) ON DELETE CASCADE;
UPDATE `tuefind_cms_pages_history` tcph JOIN `tuefind_cms_pages` tcp ON tcph.cms_id = tcp.id SET tcph.subsystem_id = tcp.subsystem_id;
ALTER TABLE `tuefind_cms_pages_history` MODIFY COLUMN `subsystem_id` INT UNSIGNED NOT NULL AFTER `cms_id`;

ALTER TABLE `tuefind_cms_pages_history` ADD COLUMN `comment` TEXT DEFAULT NULL AFTER `subsystem_id`;

ALTER TABLE `tuefind_cms_pages_history` DROP CONSTRAINT `fk_history_cms`;
ALTER TABLE `tuefind_cms_pages_history` ADD CONSTRAINT `fk_history_cms` FOREIGN KEY (`cms_id`) REFERENCES `tuefind_cms_pages` (`id`) ON DELETE SET NULL;
