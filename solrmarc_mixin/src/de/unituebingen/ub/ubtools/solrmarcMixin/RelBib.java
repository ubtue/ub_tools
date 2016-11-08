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

    /* Predicate to check whether an IxTheo-Notation is relevant for RelBib
    */
    
    protected Boolean isNotRelevantForRelBib(String notation) {
        Matcher matcher = RELBIB_POSITIVE_MATCH_PATTERN.matcher(notation);
        return !matcher.matches();
    }


    /* Like the IxTheo analog but filter out all notations not relevant for RelBib
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

    public String getIsDefinitelyReligiousStudies(final Record record) {
        final List<VariableField> _084Fields = record.getVariableFields("084");
        for (final VariableField _084Field : _084Fields) {
            final DataField dataField = (DataField)_084Field;
            final Subfield subfield2 = dataField.getSubfield('2');
            if (subfield2 == null || !subfield2.getData().equals("ssgn"))
                continue;

            final Subfield subfieldA = dataField.getSubfield('a');
            if (subfieldA != null && subfieldA.getData().equals("0"))
                return TRUE;
        }

        return FALSE;
    }

    public String getIsProbablyReligiousStudies(final Record record) {
        final List<VariableField> _191Fields = record.getVariableFields("191");
        for (final VariableField _191Field : _191Fields) {
            final DataField dataField = (DataField)_191Field;
            final Subfield subfieldA = dataField.getSubfield('a');
            if (subfieldA != null && subfieldA.getData().equals("1"))
                return TRUE;
        }

        return FALSE;
    }


    public String getIsReligiousStudies(final Record record) {
        return getIsDefinitelyReligiousStudies(record).equals(TRUE) || getIsProbablyReligiousStudies(record).equals(TRUE) ?
               TRUE : FALSE;
    }

}
