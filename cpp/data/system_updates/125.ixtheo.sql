UPDATE vufind.user SET tuefind_institution=ixtheo_institution WHERE ixtheo_institution IS NOT NULL AND ixtheo_institution != '';
ALTER TABLE vufind.user DROP COLUMN ixtheo_institution;
