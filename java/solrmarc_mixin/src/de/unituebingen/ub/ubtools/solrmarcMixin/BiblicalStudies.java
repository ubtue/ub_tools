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
    public String getIsBiblicalStudies(final Record record) {
        return record.getVariableFields("BIB").isEmpty() ? "false" : "true";
    }
}
