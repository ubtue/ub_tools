package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.search.DocIdSet;
import org.apache.lucene.search.Filter;
import org.apache.lucene.util.Bits;

import java.io.IOException;


public class BibleRangeFilter extends Filter {

    private final static String FIELD = "bible_ranges";
    private final Range[] ranges;
    private final boolean isSearchingForBooks;

    public BibleRangeFilter(final BibleRange[] ranges) {
        this.ranges = ranges;
        boolean isSearchingForBooks = false;
        for (final BibleRange range : ranges) {
            isSearchingForBooks |= range.isBook();
        }
        this.isSearchingForBooks = isSearchingForBooks;
    }

    public boolean matches(final BibleRange[] documentRanges) {
        final BibleRange[] fieldRanges = isSearchingForBooks ? documentRanges : BibleRange.removeBooks(documentRanges);
        return fieldRanges.length != 0 && Range.hasIntersections(ranges, fieldRanges);
    }

    @Override
    public DocIdSet getDocIdSet(final LeafReaderContext context, final Bits acceptDocs)
            throws IOException {
        //final BinaryDocValues values = FieldCache.DEFAULT.getTerms(context.reader(), FIELD, false);
        return null; //new FastDocIdSet(context.reader().maxDoc(), acceptDocs, values);
    }

    @Override
    public String toString(final String s) {
        return s;
    }
/*
    public class FastDocIdSet extends FieldCacheDocIdSet {
        private final BinaryDocValues values;

        public FastDocIdSet(final int maxDoc, final Bits acceptDocs, final BinaryDocValues values) {
            super(maxDoc, acceptDocs);
            this.values = values;
        }
        @Override
        protected final boolean matchDoc(final int docId) {
            final BytesRef ref = values.get(docId);
            final String dbField = ref.utf8ToString();
            if (dbField.isEmpty()) {
                return false;
            }
            final BibleRange[] documentRanges = BibleRangeParser.getRangesFromDatabaseField(dbField);
            return matches(documentRanges);
        }
    }*/
}
