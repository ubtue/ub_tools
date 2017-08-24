<?php

/**
 * require all PHP files in a directory (once)
 *
 * @param string $dir
 * @param bool $recursive
 */
function require_dir($dir, $recursive=false) {
    $handle = opendir($dir);
    while (false !== ($subpath = readdir($handle))) {
        if (($subpath != '.') && ($subpath != '..')) {
            $path = $dir . $subpath;
            if (is_dir($path)) {
                if ($recursive) {
                    require_dir($path);
                }
            } elseif (is_file($path) && preg_match('"\.php$"i', $path)) {
                require_once($path);
            }
        }
    }
    closedir($handle);
}