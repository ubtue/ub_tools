<?php

// This is the download page for result files (e.g. .mrc or .xml)
require_once('inc.php');
if (isset($_GET['id'])) {
    $path = DIR_TMP . $_GET['id'];
    if (is_file($path)) {
        if(preg_match('"\.xml$"i', $path)) {
            header('Content-type: application/xml');
        }
        sendfile_chunked($path);
    }
}

?>