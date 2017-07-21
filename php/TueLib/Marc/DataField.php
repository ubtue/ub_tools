<?php

/**
 * This class represents a data field
 *
 * @author      Mario Trojan <mario.trojan@uni-tuebingen.de>
 * @copyright   2017 Universtitätsbibliothek Tübingen
 */
namespace TueLib\Marc;
class DataField extends AbstractField {
    protected $_tag;
    protected $_indicator1  = ' ';
    protected $_indicator2  = ' ';
    protected $_subFields   = [];

    function __construct($tag=null, $indicator1=' ', $indicator2=' ') {
        $this->_tag          = $tag;
        $this->_indicator1   = $indicator1;
        $this->_indicator2   = $indicator2;
    }

    static public function ImportFromDomElement(\DOMElement $element) {
        $field = new DataField($element->getAttribute('tag'), $element->getAttribute('ind1'), $element->getAttribute('ind2'));
        foreach ($element->childNodes as $child) {
            if ($child instanceof \DOMElement && $child->tagName == 'marc:subfield') {
                $field->_subFields[] = SubField::ImportFromDomElement($child);
            }
        }
        return $field;
    }

    public function ExportToXmlString() {
        $string = '<marc:datafield tag="'.$this->_tag.'" ind1="'.$this->_indicator1.'" ind2="'.$this->_indicator2.'">' . PHP_EOL;
        foreach ($this->_subFields as $subfield) {
            $string .= $subfield->ExportToXmlString() . PHP_EOL;
        }
        $string .= '</marc:datafield>' . PHP_EOL;
        return $string;
    }

    public function AddSubField(SubField $subfield) {
        $this->_subFields[] = $subfield;
    }

    public function GetTag() {
        return $this->_tag;
    }

    public function GetIndicator1() {
        return $this->_indicator1;
    }

    public function GetIndicator2() {
        return $this->_indicator2;
    }

    public function GetSubFields() {
        return $this->_subFields;
    }
}

?>