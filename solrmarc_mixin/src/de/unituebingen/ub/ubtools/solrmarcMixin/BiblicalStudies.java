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
 
    final static String BIBSTUDIES_IXTHEO_PATTERN="^[H][A-Z].*|.*:[H][A-Z].*";
    Pattern bibStudiesIxTheoPattern = Pattern.compile(BIBSTUDIES_IXTHEO_PATTERN);

    public String getIsBiblicalStudiesIxTheoNotation(final Record record) {
        final List<VariableField> _652Fields = record.getVariableFields("652");
        for (final VariableField _652Field : _652Fields) {
            final DataField dataField = (DataField) _652Field;
            for (final Subfield subfieldA : dataField.getSubfields('a')) {
                if (subfieldA == null)
                    continue;
                Matcher matcher = bibStudiesIxTheoPattern.matcher(subfieldA.getData());
                if (matcher.matches())
                    return TRUE;
            }
        }
        return FALSE;
    }

    public String getIsBiblicalStudies(final Record record) {
        return getIsBiblicalStudiesIxTheoNotation(record);
    }
}
