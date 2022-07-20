package de.uni_tuebingen.ub.ixTheo.canonesDateRangeSearch;


import org.apache.lucene.document.Document;
import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.search.Weight;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeWeight;


public class CanonesDateRangeWeight extends RangeWeight {
    private final static String FIELD = "canon_law_ranges";
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
    protected boolean matches(final Document document) {
        final String dbField = document.get(FIELD);
        final CanonesDateRange[] documentRanges = CanonesDateRangeParser.getRangesFromDatabaseField(dbField);
        final CanonesDateRange[] fieldRanges = isSearchingForCodices ? documentRanges : CanonesDateRange.removeCodices(documentRanges);
        return fieldRanges.length != 0 && Range.hasIntersections(ranges, fieldRanges);
    }

    @Override
    protected float customScore(final Document doc) {
        final String dbField = doc.get(FIELD);
        if (dbField == null || dbField.isEmpty()) {
            return NOT_RELEVANT;
        }
        final Range[] field_ranges = CanonesDateRangeParser.getRangesFromDatabaseField(dbField);
        return Range.getMatchingScore(field_ranges, ranges);
    }

    @Override
    public final boolean isCacheable(LeafReaderContext context) {
        return false;
    }
}
