ALTER TABLE vufind.user MODIFY COLUMN ixtheo_user_type ENUM('ixtheo', 'relbib', 'bibstudies', 'churchlaw') NOT NULL DEFAULT 'ixtheo';
ALTER TABLE vufind.user DROP INDEX `username`;
CREATE UNIQUE INDEX `subsystem_username` ON vufind.user (`ixtheo_user_type`, `username`);
CREATE UNIQUE INDEX `subsystem_email` ON vufind.user (`ixtheo_user_type`, `email`);
