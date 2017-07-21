<?php

/**
 * This class represents a control field
 *
 * @author      Mario Trojan <mario.trojan@uni-tuebingen.de>
 * @copyright   2017 Universtitätsbibliothek Tübingen
 */
namespace TueLib\Marc;
class ControlField extends AbstractField {
    protected $_tag;
    protected $_value;

    function __construct($tag=null, $value=null) {
        $this->_tag      = $tag;
        $this->_value    = $value;
    }

    static public function ImportFromDomElement(\DOMElement $element) {
        return new ControlField($element->getAttribute('tag'), $element->nodeValue);
    }

    public function ExportToXmlString() {
        return '<marc:controlfield tag="' . $this->_tag . '">' . htmlspecialchars($this->_value) . '</marc:controlfield>';
    }

    public function GetTag() {
        return $this->_tag;
    }

    public function GetValue() {
        return $this->_value;
    }
}

?>