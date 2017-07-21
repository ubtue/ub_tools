<?php

/**
 * This class represents a leader field
 *
 * @author      Mario Trojan <mario.trojan@uni-tuebingen.de>
 * @copyright   2017 Universtitätsbibliothek Tübingen
 */
namespace TueLib\Marc;
class Leader extends AbstractField {
    protected $_value;

    function __construct($value=null) {
        $this->_value        = $value;
    }

    static public function ImportFromDomElement(\DOMElement $element) {
        return new Leader($element->nodeValue);
    }

    public function ExportToXmlString() {
        return '<marc:leader>' . htmlspecialchars($this->_value) . '</marc:leader>';
    }

    public function GetValue() {
        return $this->_value;
    }
}

?>