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
}
