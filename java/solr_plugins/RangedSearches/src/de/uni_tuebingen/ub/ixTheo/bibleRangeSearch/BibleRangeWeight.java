package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import java.io.IOException;
import org.apache.lucene.document.Document;
import org.apache.lucene.search.Weight;
import org.apache.lucene.search.Explanation;
import org.apache.lucene.index.LeafReaderContext;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeWeight;

public class BibleRangeWeight extends RangeWeight {
    private final boolean isSearchingForBooks;

    public BibleRangeWeight(final BibleRangeQuery query, final BibleRange[] ranges, final Weight weight) {
        super(query, ranges, weight);

        boolean isSearchingForBooksLocal = false;
        for (final BibleRange range : ranges) {
            isSearchingForBooksLocal |= range.isEntireBook();
        }
        isSearchingForBooks = isSearchingForBooksLocal;
    }

    @Override
    protected String getRangeFieldName() {
        return "bible_ranges";
    }

    @Override
    protected Range[] getRangesFromDatabaseField(final String dbField) {
        return BibleRangeParser.getRangesFromDatabaseField(dbField);
    }

    @Override
    protected boolean matches(final Document document) {
        final String dbField = document.get(getRangeFieldName());
        final BibleRange[] documentRanges = BibleRangeParser.getRangesFromDatabaseField(dbField);
        final BibleRange[] fieldRanges = isSearchingForBooks ? documentRanges : BibleRange.removeBooks(documentRanges);
        return fieldRanges.length != 0 && Range.hasIntersections(ranges, fieldRanges);
    }

    // for now it is just use for compatibility
    // @Override
    // public Explanation explain(LeafReaderContext context, int doc) throws
    // IOException {
    // return explain(context.reader().document(doc));
    // }
}
