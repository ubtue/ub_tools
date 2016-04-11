package de.unituebingen.ub.ubtools.solrmarcMixin;


import org.marc4j.marc.DataField;
import org.marc4j.marc.VariableField;
import org.marc4j.marc.Record;
import org.solrmarc.index.SolrIndexerMixin;

import java.util.*;


public class IxTheo extends SolrIndexerMixin {
    private Set<String> ixTheoNotations = null;
    private static Set<String> unassigned = Collections.singleton("[Unassigned]");

    @Override
    public void perRecordInit(final Record record) {
        super.perRecordInit(record);
        ixTheoNotations = null;
    }

    /**
     * Split the colon-separated ixTheo notation codes into individual codes and return them.
     */
    public Set<String> getIxTheoNotations(final Record record) {
        if (ixTheoNotations == null) {
            ixTheoNotations = new HashSet<>();
            final List fields = record.getVariableFields("652");
            if (fields.isEmpty()) {
                return ixTheoNotations;
            }
            final DataField data_field = (DataField) fields.iterator().next(); // We should only have one 652 field.
            final String contents = data_field.getSubfield('a').getData(); // There should always be exactly one $a subfield.
            final String[] parts = contents.split(":");
            Collections.addAll(ixTheoNotations, parts);
        }
        return ixTheoNotations;
    }

    public Set<String> getIxTheoNotationFacets(final Record record) {
        final Set<String> ixTheoNotations = getIxTheoNotations(record);
        if (ixTheoNotations.isEmpty()) {
            return unassigned;
        }
        return ixTheoNotations;
    }


   /**
     * Determine Record Format(s)
     *
     * @param record the record
     * @return format of record
     */

    public Set<String> getFormatsWithGermanHandling(final Record record){

       // We've been facing the problem that the original SolrMarc cannot deal with 
       // german descriptions in the 245h and thus assigns a wrong format 
       // for e.g. electronic resource
       // Thus we must handle this manually

       Set<String> rawFormats = new LinkedHashSet<String>();
       DataField title = (DataField) record.getVariableField("245");

       if (title != null) {
           if (title.getSubfield('h') != null) {
                if (title.getSubfield('h').getData().toLowerCase().contains("[elektronische ressource]")) {
                   rawFormats.add("Electronic");
                   return rawFormats;
                } else {
                   return indexer.getFormat(record);
                }
           }
       }

       // Catch case of empty title
       return indexer.getFormat(record);

    }


    /**
     * Determine Record Format(s)
     *
     * @param record the record
     * @return format of record
     */
    public Set getFormat(final Record record) {
        final Set<String> formats = new HashSet<>();
        Set<String> rawFormats = getFormatsWithGermanHandling(record);
        
        for (final String rawFormat : rawFormats) {
            if (rawFormat.equals("BookComponentPart") || rawFormat.equals("SerialComponentPart")) {
                formats.add("Article");
            } else {
                formats.add(rawFormat);
            }
        }

        final List<VariableField> _655Fields = record.getVariableFields("655");
	for (final VariableField _655Field : _655Fields) {
	    final DataField dataField = (DataField)_655Field;
	    if (dataField.getIndicator1() == ' ' && dataField.getIndicator2() == '7'
		&& dataField.getSubfield('a').getData().startsWith("Rezension"))
            {
		formats.clear();
		formats.add("Review");
		break;
	    }
	}

        final List<VariableField> _935Fields = record.getVariableFields("935");
	for (final VariableField _935Field : _935Fields) {
	    final DataField dataField = (DataField)_935Field;
	    if (dataField.getIndicator1() == ' ' && dataField.getIndicator2() == '7'
		&& dataField.getSubfield('c').getData().equals("uwre"))
            {
		formats.clear();
		formats.add("Review");
		break;
	    }
	}

        return formats;
    }
}
