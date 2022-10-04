package de.uni_tuebingen.ub.ixTheo.canonesDateRangeSearch;


import org.apache.lucene.document.Document;
import org.apache.lucene.search.Weight;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeWeight;


public class CanonesDateRangeWeight extends RangeWeight {
    private final boolean isSearchingForCodices;

    public CanonesDateRangeWeight(final CanonesDateRangeQuery query, final CanonesDateRange[] ranges, final Weight weight) {
        super(query, ranges, weight);

        boolean isSearchingForCodicesLocal = false;
        for (final CanonesDateRange range : ranges) {
            isSearchingForCodicesLocal |= range.isEntireCodex();
        }
        isSearchingForCodices = isSearchingForCodicesLocal;
    }

    @Override
    protected String getRangeFieldName() {
        return "canon_law_ranges";
    }

    @Override
    protected Range[] getRangesFromDatabaseField(final String dbField) {
        return CanonesDateRangeParser.getRangesFromDatabaseField(dbField);
    }

    @Override
    protected boolean matches(final Document document) {
        final String dbField = document.get(getRangeFieldName());
        final CanonesDateRange[] documentRanges = CanonesDateRangeParser.getRangesFromDatabaseField(dbField);
        final CanonesDateRange[] fieldRanges = isSearchingForCodices ? documentRanges : CanonesDateRange.removeCodices(documentRanges);
        return fieldRanges.length != 0 && Range.hasIntersections(ranges, fieldRanges);
    }
}
