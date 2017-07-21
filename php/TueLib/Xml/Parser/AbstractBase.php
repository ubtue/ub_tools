<?php

/**
 * Abstract XML parser class, as wrapper around event-based PHP XML parser
 * Can be used for reading only or for reading/writing at the same time
 *
 * @author      Mario Trojan <mario.trojan@uni-tuebingen.de>
 * @copyright   2017 Universtitätsbibliothek Tübingen
 */
namespace TueLib\Xml\Parser;
abstract class AbstractBase {

    protected $_parser;
    protected $_handle_src;
    protected $_handle_dst;
    protected $_written_header = false;

    function __construct() {
        $this->_init();
    }

    function __destruct() {
        if (!is_null($this->_parser)) {
            @xml_parser_free($this->_parser);
            $this->_parser = null;
        }
        if (!is_null($this->_handle_src)) {
            fclose($this->_handle_src);
            $this->_handle_src = null;
        }
        if (!is_null($this->_handle_dst)) {
            fclose($this->_handle_dst);
            $this->_handle_dst = null;
        }
    }

    protected function _init() {
        // init parser
        $this->_parser          = xml_parser_create();
        $this->_handle_src      = null;
        $this->_handle_dst      = null;
        $this->_written_header  = false;

        // register handlers
        xml_set_character_data_handler($this->_parser, [$this, '_rawHandleCharacterData']);
        xml_set_default_handler($this->_parser, [$this, '_rawHandleDefault']);
        xml_set_element_handler($this->_parser, [$this, '_rawHandleElementStart'], [$this, '_rawHandleElementEnd']);
        xml_set_processing_instruction_handler($this->_parser, [$this, '_rawHandleProcessingInstruction']);

        // set options
        xml_parser_set_option($this->_parser, XML_OPTION_CASE_FOLDING, false);
    }

    public function parse($xml_path_src, $xml_path_dst=null) {
        $this->_handle_src = fopen($xml_path_src, 'r');
        if ($xml_path_dst != null) {
            $this->_handle_dst = fopen($xml_path_dst, 'w');
        }

        while (!feof($this->_handle_src)) {
            $data = fgets($this->_handle_src);
            xml_parse($this->_parser, $data, feof($this->_handle_src));
        }
    }

    /* raw callback wrappers */
    protected function _rawHandleCharacterData($parser, $data) {
        $this->_handleCharacterData($data);
    }

    protected function _rawHandleDefault($parser, $data) {
        $this->_handleDefault($data);
    }

    protected function _rawHandleElementStart($parser, $name, $attribs) {
        $this->_handleElementStart($name, $attribs);
    }

    protected function _rawHandleElementEnd($parser, $name) {
        $this->_handleElementEnd($name);
    }

    protected function _rawHandleProcessingInstruction($parser, $target, $data) {
        $this->_handleProcessingInstruction($target, $data);
    }

    /* callback functions, can be overwritten in subclass */
    protected function _handleCharacterData($data) {
        $this->_write($this->_toStringEscaped($data));
    }

    protected function _handleDefault($data) {
        $this->_write($this->_toStringEscaped($data));
    }

    protected function _handleElementStart($name, $attribs) {
        $this->_write($this->_toStringElementStart($name, $attribs));
    }

    protected function _handleElementEnd($name) {
        $this->_write($this->_toStringElementEnd($name));
    }

    protected function _handleProcessingInstruction($target, $data) {
        $this->_write($this->_toStringProcessingInstruction($target, $data));
    }

    /* helper functions for xml string processing */
    protected function _toStringElementStart($name, $attribs) {
        $tag = '<' . $name;
        if (is_array($attribs)) {
            foreach($attribs as $key => $value) {
                $tag .= ' ' . $key . '="' . $value . '"';
            }
        }
        $tag .= '>';
        return $tag;
    }

    protected function _toStringElementEnd($name) {
        return '</' . $name . '>';
    }

    protected function _toStringEscaped($data) {
        return htmlspecialchars($data, ENT_QUOTES | ENT_XML1);
    }

    protected function _toStringProcessingInstruction($target, $data) {
        return '<?' . $target . ' ' . $data . '?>';
    }

    // if write mode active, write data to output file
    protected function _write($data) {
        if ($this->_handle_dst != null) {
            if ($this->_written_header == false) {
                $this->_written_header = true;
                fwrite($this->_handle_dst, '<?xml version="1.0" encoding="UTF-8"?>' . PHP_EOL);
            }
            fwrite($this->_handle_dst, $data);
        }
    }
}

?>