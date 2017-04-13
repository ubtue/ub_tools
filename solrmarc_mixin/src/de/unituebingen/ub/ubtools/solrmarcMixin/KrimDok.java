package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.marc4j.marc.Subfield;
import org.marc4j.marc.VariableField;
import org.solrmarc.index.SolrIndexerMixin;

public class KrimDok extends SolrIndexerMixin {
    private final static DateFormat recordingDateFormat = new SimpleDateFormat("yyyyMM");

    /**
     * get thew records's recoding date from a LOK-tagged field
     *
     * @param record  the record
     * @return The date in ISO-8601 format
     */
    public String getLocalRecordingDate(final Record record) {
        for (final VariableField variableField : record.getVariableFields("LOK")) {
            final DataField lokfield = (DataField) variableField;
            final Subfield subfield0 = lokfield.getSubfield('0');
            if (subfield0 != null && subfield0.getData().equals("938  ")) {
                final Subfield subfield_a = lokfield.getSubfield('a');
                if (subfield_a != null) {
                    final String dateCandidate = subfield_a.getData();
                    try {
                        // We only use this check that we have a valid date.
                        final Date recodingDate = recordingDateFormat.parse("20" + dateCandidate);

                        return "20" + dateCandidate.substring(0, 2) + "-" + dateCandidate.substring(2, 4) + "-01";
                    } catch (Exception e) {
                        System.err.println("strange local recoding date: " + dateCandidate);
                    }
                }
            }
        }
        return null;
    }
}
