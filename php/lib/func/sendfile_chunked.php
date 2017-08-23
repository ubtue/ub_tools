<?php

/**
 * Read a file and display its content chunk by chunk (useful for browser)
 *
 * @param string $path
 * @param int $chunk_size
 * @return int Number of bytes read
 */
function sendfile_chunked($path, $chunk_size=1024*1024) {
    $buffer = '';
    $cnt    = 0;
    $handle = fopen($path, 'rb');

    if ($handle === false) {
        return false;
    }

    while (!feof($handle)) {
        $buffer = fread($handle, $chunk_size);
        echo $buffer;
        ob_flush();
        flush();

        if ($retbytes) {
            $cnt += strlen($buffer);
        }
    }
    fclose($handle);
    return $cnt;
}