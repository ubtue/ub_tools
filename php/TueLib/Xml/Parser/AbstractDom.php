<?php

/**
 * Abstract buffered XML parser class with DOM callback
 *
 * Idea is to parse event-based to save memory, but to buffer content
 * based on tagnames until you have a full element.
 *
 * This parser will deliver a DOMElement in each callback.
 * You may manipulate the element (or replace it with some other element, or just
 * remove it) to manipulate the output file.
 *
 * @author      Mario Trojan <mario.trojan@uni-tuebingen.de>
 * @copyright   2017 Universtitätsbibliothek Tübingen
 */
namespace TueLib\Xml\Parser;
require_once('AbstractBuffered.php');
abstract class AbstractDom extends AbstractBuffered {

    /**
     * get contens of buffer as DOM document object.
     * @return DOMDocument
     */
    protected function _bufferGetDomDocument() {
        $dom = new \DOMDocument();
        $xml_fragment = $this->_toStringElementStart($this->_root_element_details['name'], $this->_root_element_details['attribs']) . $this->_buffer . $this->_toStringElementEnd($this->_root_element_details['name']);
        $dom->loadXML($xml_fragment);
        return $dom;
    }

    /**
     * check if buffer contains matching element for callback
     * and trigger callback with DOM object
     */
    protected function _bufferFlush() {
        if ($this->_bufferContainsCallback()) {
            $dom = $this->_bufferGetDomDocument();
            $root = $dom->documentElement->firstChild;
            $root = $this->_callbackDomElement($root);

            if ($root == null) $this->_bufferReset();
            else $this->_buffer = $root->ownerDocument->saveXML($root);
        }
        parent::_bufferFlush();
    }

    /**
     * this abstract function needs to be overwritten in child class
     * to process the single callback element as DOMElement
     *
     * @param DOMElement $element
     *
     * @return DOMElement
     */
    abstract protected function _callbackDomElement(\DOMElement $element);
}
?>