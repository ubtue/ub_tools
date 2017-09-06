<?php

/**
 * Flush output buffer and send contents to browser
 * Necessary, because ob_flush is not enough in apache context
 */
function ob_flush_real() {
    ob_flush();
    flush();
}