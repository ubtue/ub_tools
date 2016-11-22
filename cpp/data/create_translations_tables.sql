CREATE TABLE vufind_translations (
  token VARCHAR(255) NOT NULL,
  language_code CHAR(3) NOT NULL,
  translation VARCHAR(1024) NOT NULL,
  UNIQUE KEY token_language_code (token,language_code),
  KEY vufind_translations_idx_token (token),
  KEY vufind_translations_idx_language_code (language_code)
) DEFAULT CHARSET=utf8;


CREATE TABLE keyword_translations (
  ppn CHAR(9) NOT NULL,
  gnd_code CHAR(10) NOT NULL,
  language_code CHAR(3) NOT NULL,
  translation VARCHAR(1024) NOT NULL,
  status ENUM('reliable', 'unreliable', 'new') NOT NULL,
  KEY keyword_translations_idx_ppn (ppn),
  KEY keyword_translations_idx_language_code (language_code)
) DEFAULT CHARSET=utf8;
