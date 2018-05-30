package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.List;
import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.marc4j.marc.VariableField;
import org.marc4j.marc.*;
import org.solrmarc.index.SolrIndexerMixin;
import org.solrmarc.tools.Utils;

public class BiblicalStudies extends IxTheo {

    final static String TRUE = "true";
    final static String FALSE = "false";
 
    public String getIsBiblicalStudies(final Record record) {
        final List<VariableField> _BIBFields = record.getVariableFields("BIB");
        return !_BIBFields.isEmpty() ? TRUE : FALSE;
    }
}
