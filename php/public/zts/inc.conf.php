<?php

// this is the main configuration file, containing directories and so on

// internal PHP library
define('DIR_LIB', '/usr/local/ub_tools/php/lib/');

// tmp directory (for CLI processing + downloads)
define('DIR_TMP', '/tmp/Zts/');

// max script runtime in seconds
define('MAX_SCRIPT_RUNTIME', 600);
ini_set('max_input_time', MAX_SCRIPT_RUNTIME);
ini_set('max_execution_time', MAX_SCRIPT_RUNTIME);

// Musterdatei für Zotero Crawler:
define('ZOTERO_FILE_CRAWLER', '/usr/local/ub_tools/cpp/data/zotero_crawler.conf');

// get and define environment variable if exists, else use default value
// this is especially useful for docker container
function DefineEnvVar($key, $default) {
    $value = getenv($key);
    if ($value != '') {
        define($key, $value);
    } else {
        define($key, $default);
    }
}

// URL to Zotero server for zts_client
DefineEnvVar('ZOTERO_TRANSLATION_SERVER_URL', 'http://ub28.uni-tuebingen.de:1969');

// URL to web proxy for zts_client
DefineEnvVar('ZOTERO_PROXY_SERVER', 'nu.ub.uni-tuebingen.de:3128');


// Create own tempdir if not exists
if (!is_dir(DIR_TMP)) mkdir(DIR_TMP);
