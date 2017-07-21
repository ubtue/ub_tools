<?php

/**
 * This class represents a subfield (of a data field)
 *
 * @author      Mario Trojan <mario.trojan@uni-tuebingen.de>
 * @copyright   2017 Universtitätsbibliothek Tübingen
 */
namespace TueLib\Marc;
class SubField extends AbstractField {
    protected $_code;
    protected $_value;

    function __construct($code=null, $value=null) {
        $this->_code     = $code;
        $this->_value    = $value;
    }

    static public function ImportFromDomElement(\DOMElement $element) {
        return new SubField($element->getAttribute('code'), $element->nodeValue);
    }

    public function ExportToXmlString() {
        return '<marc:subfield code="'.$this->_code.'">' . htmlspecialchars($this->_value) . '</marc:subfield>';
    }

    public function GetCode() {
        return $this->_code;
    }

    public function GetValue() {
        return $this->_value;
    }
}

?>