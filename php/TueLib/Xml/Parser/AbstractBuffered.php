<?php

/**
 * Abstract buffered XML parser class
 *
 * Idea is to parse event-based to save memory, but to buffer content
 * based on tagnames until you have a full element.
 * e.g. use "marc:record" in _callback_tagnames to get a callback if
 * the corresponding endtag is reached. Then you have the full element in buffer,
 * are able to manipulate it (e.g. via DOM), and so on.
 *
 * If you manipulate buffer during callback, you will be able to
 * write your result to outfile.
 *
 * @author      Mario Trojan <mario.trojan@uni-tuebingen.de>
 * @copyright   2017 Universtitätsbibliothek Tübingen
 */
namespace TueLib\Xml\Parser;
require_once('AbstractBase.php');
abstract class AbstractBuffered extends AbstractBase {

    /**
     * This buffer contains all characters which have not been processed
     * by callbacks yet
     * @var string
     */
    protected $_buffer = '';

    /**
     * These tagnames can be overwritten. The callback function will be called
     * after each endtag with one of these tagnames.
     * (warning: please avoid nested callbacks if possible!)
     * @var array
     */
    protected $_callback_tagnames = [];

    /**
     * Counter, how many callbacks have been made
     * @var int
     */
    protected $_callback_count = 0;

    /**
     * container for namespaces. workaround, because
     * xml_set_start_namespace_decl_handler unfortunately doesnt work
     * @var array
     */
    protected $_namespaces = [];

    /**
     * Array containing name and attribs of the root element as soon as it has
     * been parsed.
     * @var array
     */
    protected $_root_element_details;

    /**
     * Simply add data to buffer
     * @param string $data
     */
    protected function _bufferAdd($data) {
        $this->_buffer .= $data;
    }

    /**
     * Does the buffer start with a callback tagname?
     * Useful to determine if we have a full element in buffer.
     * @param string $data
     */
    protected function _bufferContainsCallback() {
        $callback_tagnames = implode('|', $this->_callback_tagnames);
        $pattern = '"^<'.$callback_tagnames.'[^/>]*(>|/>)"';
        return preg_match($pattern, $this->_buffer);
    }

    /**
     * flush buffer.
     * write buffer to output file (if in write mode) and reset buffer.
     */
    protected function _bufferFlush() {
        $this->_write($this->_buffer);
        $this->_bufferReset();
    }

    /**
     * print buffer contents (for debugging)
     */
    protected function _bufferPrint() {
        print $this->_buffer . PHP_EOL;
    }

    /**
     * reset buffer (without writing, just reset!)
     */
    protected function _bufferReset() {
        $this->_buffer = '';
    }

    /**
     * get number of callbacks performed
     * @return int
     */
    public function GetCallbackCount() {
        return $this->_callback_count;
    }

    /**
     * normal parser callback. simply add data to buffer.
     * @param string $data
     */
    protected function _handleCharacterData($data) {
        $this->_bufferAdd($this->_toStringEscaped($data));
    }

    /**
     * normal parser callback. simply add data to buffer.
     * @param string $data
     */
    protected function _handleDefault($data) {
        $this->_bufferAdd($this->_toStringEscaped($data));
    }

    /**
     * parser callback. Check if tagname is relevant for callbacks and start
     * buffering if needed.
     * @param string $name
     * @param array $attribs
     */
    protected function _handleElementStart($name, $attribs) {
        // Detect namespaces. Workaround cause handleNamespaceStartDeclaration isnt called
        foreach ($attribs as $key => $value) {
            if (preg_match('":"', $key)) {
                $this->_namespaces[$key] = $value;
            }
        }

        // Cache root element for later document fragments if needed (namespaces, etc.)
        if ($this->_root_element_details == null) {
            $this->_root_element_details = ['name' => $name, 'attribs' => $attribs];
        }

        // Buffering
        if (in_array($name, $this->_callback_tagnames)) {
            $this->_bufferFlush();
        }
        $this->_bufferAdd($this->_toStringElementStart($name, $attribs));
    }

    /**
     * parser callback. Check if tagname is relevant for callbacks, call internal
     * callback if necessary. Also flush buffer after processing.
     * @param string $name
     */
    protected function _handleElementEnd($name) {
        $this->_bufferAdd($this->_toStringElementEnd($name));
        if (in_array($name, $this->_callback_tagnames) || $name == $this->_root_element_details['name']) {
            $this->_bufferFlush();

            if (in_array($name, $this->_callback_tagnames)) {
                $this->_callback_count++;
            }
        }
    }

    /**
     * parser callback. build processing instruction and put it onto buffer.
     * @param string $target
     * @param string $data
     */
    protected function _handleProcessingInstruction($target, $data) {
        $this->bufferAdd($this->_toStringProcessingInstruction($target, $data));
    }
}
?>