package de.uni_tuebingen.ub.ixTheo.bibleDateRangeSearch;


import org.apache.lucene.document.Document;
import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.search.Weight;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeWeight;


public class BibleDateRangeWeight extends RangeWeight {
    private final static float NOT_RELEVANT = Float.NEGATIVE_INFINITY;
    private final static String FIELD = "bible_ranges";
    private final boolean isSearchingForBooks;

    public BibleDateRangeWeight(final BibleDateRangeQuery query, final BibleDateRange[] ranges, final Weight weight) {
        super(query, ranges, weight);

        boolean isSearchingForBooksLocal = false;
        for (final BibleDateRange range : ranges) {
            isSearchingForBooksLocal |= range.isEntireBook();
        }
        isSearchingForBooks = isSearchingForBooksLocal;
    }

    @Override
    protected boolean matches(final Document document) {
        final String dbField = document.get(FIELD);
        final BibleDateRange[] documentRanges = BibleDateRangeParser.getRangesFromDatabaseField(dbField);
        final BibleDateRange[] fieldRanges = isSearchingForBooks ? documentRanges : BibleDateRange.removeBooks(documentRanges);
        return fieldRanges.length != 0 && Range.hasIntersections(ranges, fieldRanges);
    }

    @Override
    protected float customScore(final Document doc) {
        final String dbField = doc.get(FIELD);
        if (dbField == null || dbField.isEmpty()) {
            return NOT_RELEVANT;
        }
        final Range[] field_ranges = BibleDateRangeParser.getRangesFromDatabaseField(dbField);
        return Range.getMatchingScore(field_ranges, ranges);
    }

    @Override
    public final boolean isCacheable(LeafReaderContext context) {
        return false;
    }
}
