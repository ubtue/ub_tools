<?php

/**
 * Read a file and display its content chunk by chunk (useful for browser)
 *
 * @param string $path
 * @param bool $retbytes        return num. bytes delivered like readfile() does.
 * @param int $chunk_size
 * @return mixed
 */
function sendfile_chunked($path, $retbytes=true, $chunk_size=1024*1024) {
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

    $status = fclose($handle);

    if ($retbytes && $status) {
        return $cnt;
    }

    return $status;
}