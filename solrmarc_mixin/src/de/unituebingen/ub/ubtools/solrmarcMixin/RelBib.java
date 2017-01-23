package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.*;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.Reader.*;
import java.io.*;
import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.marc4j.marc.VariableField;
import org.marc4j.marc.*;
import org.solrmarc.index.SolrIndexerMixin;
import org.solrmarc.tools.Utils;

public class RelBib extends IxTheo {

    protected final static Pattern RELBIB_POSITIVE_MATCH_PATTERN = Pattern.compile("^A.*|^B.*|^KB.*|^T.*|^V.*|^X.*|^Z.*|^.*Unassigned.*");

    /*
     * Predicate to check whether an IxTheo-Notation is relevant for RelBib
     */

    protected Boolean isNotRelevantForRelBib(String notation) {
        Matcher matcher = RELBIB_POSITIVE_MATCH_PATTERN.matcher(notation);
        return !matcher.matches();
    }

    /*
     * Like the IxTheo analog but filter out all notations not relevant for
     * RelBib
     */

    public Set<String> getRelBibNotationFacets(final Record record) {
        Set<String> relBibNotations = getIxTheoNotationFacets(record);
        Iterator<String> relBibNotationsIter = relBibNotations.iterator();
        while (relBibNotationsIter.hasNext()) {
            if (isNotRelevantForRelBib(relBibNotationsIter.next()))
                relBibNotationsIter.remove();
        }
        return relBibNotations;
    }

    final static String TRUE = "true";
    final static String FALSE = "false";

    // Integrate DDC 220-289
    String RELSTUDIES_DDC_RANGE_PATTERN = "2[2-8][0-9]\\.?[^.]*";
    Pattern relStudiesDDCPattern = Pattern.compile(RELSTUDIES_DDC_RANGE_PATTERN);

    public String getIsNotReligiousStudiesDDC(final Record record) {
        final List<VariableField> _082Fields = record.getVariableFields("082");
        for (final VariableField _082Field : _082Fields) {
            final DataField dataField = (DataField) _082Field;
            final Subfield subfieldA = dataField.getSubfield('a');
            if (subfieldA == null)
                continue;
            Matcher matcher = relStudiesDDCPattern.matcher(subfieldA.getData());
            if (matcher.matches())
                return TRUE;
        }
        return FALSE;
    }

    // Integrate IxTheo Notations A*.B*,T*,V*,X*,Z*
    String RELSTUDIES_IXTHEO_PATTERN = "^[ABTVXZ][A-Z].*|.*:[ABTVXZ][A-Z].*";
    Pattern relStudiesIxTheoPattern = Pattern.compile(RELSTUDIES_IXTHEO_PATTERN);

    public String getIsReligiousStudiesIxTheo(final Record record) {
        final List<VariableField> _652Fields = record.getVariableFields("652");
        for (final VariableField _652Field : _652Fields) {
            final DataField dataField = (DataField) _652Field;
            final Subfield subfieldA = dataField.getSubfield('a');
            if (subfieldA == null)
                continue;
            Matcher matcher = relStudiesIxTheoPattern.matcher(subfieldA.getData());
            if (matcher.matches())
                return TRUE;
        }
        return FALSE;
    }

    public String getIsReligiousStudiesSSGN(final Record record) {
        final List<VariableField> _084Fields = record.getVariableFields("084");
        for (final VariableField _084Field : _084Fields) {
            final DataField dataField = (DataField) _084Field;
            for (final Subfield subfield2 : dataField.getSubfields('2')) {
                if (subfield2 == null || !subfield2.getData().equals("ssgn"))
                    continue;
                
                for (final Subfield subfieldA : dataField.getSubfields('a')) {
                    if (subfieldA != null && subfieldA.getData().equals("0"))
                        return TRUE;
                }
            }
        }
        return FALSE;
    }

    public String getIsDefinitelyReligiousStudies(final Record record) {

        return (getIsReligiousStudiesSSGN(record).equals(TRUE) || getIsReligiousStudiesIxTheo(record).equals(TRUE)) && getIsNotReligiousStudiesDDC(record).equals(FALSE) ? TRUE : FALSE;
    }

    public String getIsProbablyReligiousStudies(final Record record) {
        final List<VariableField> _191Fields = record.getVariableFields("191");
        for (final VariableField _191Field : _191Fields) {
            final DataField dataField = (DataField) _191Field;
            final Subfield subfieldA = dataField.getSubfield('a');
            if (subfieldA != null && subfieldA.getData().equals("1"))
                return TRUE;
        }

        return FALSE;
    }

    public String getIsReligiousStudies(final Record record) {
        return getIsDefinitelyReligiousStudies(record).equals(TRUE) || getIsProbablyReligiousStudies(record).equals(TRUE) ? TRUE : FALSE;
    }

}
