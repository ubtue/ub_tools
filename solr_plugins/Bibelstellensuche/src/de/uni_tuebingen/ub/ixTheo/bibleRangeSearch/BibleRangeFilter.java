package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import java.io.IOException;

import org.apache.lucene.index.AtomicReaderContext;
import org.apache.lucene.index.BinaryDocValues;
import org.apache.lucene.search.DocIdSet;
import org.apache.lucene.search.FieldCache;
import org.apache.lucene.search.FieldCacheDocIdSet;
import org.apache.lucene.search.Filter;
import org.apache.lucene.util.Bits;
import org.apache.lucene.util.BytesRef;

public class BibleRangeFilter extends Filter {

    private final static String FIELD = "bible_ranges";
    private final Range[] ranges;

    public BibleRangeFilter(final Range[] ranges) {
        this.ranges = ranges;
    }

    @Override
    public DocIdSet getDocIdSet(final AtomicReaderContext context, final Bits acceptDocs)
            throws IOException {
        final BinaryDocValues values = FieldCache.DEFAULT.getTerms(context.reader(), FIELD, false);
        return new FastDocIdSet(context.reader().maxDoc(), acceptDocs, values);
    }

    class FastDocIdSet extends FieldCacheDocIdSet {
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
            final Range[] fieldRanges = BibleRangeParser.getRangesFromDatabaseField(dbField);
            return Range.doAllSourceRangesIntersectsSomeTargetRanges(
                    ranges, fieldRanges);
        }
    }
}
