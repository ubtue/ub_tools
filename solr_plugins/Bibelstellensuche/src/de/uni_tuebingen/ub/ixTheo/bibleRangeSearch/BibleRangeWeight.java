package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import com.carrotsearch.hppc.IntFloatHashMap;
import com.carrotsearch.hppc.IntFloatMap;
import org.apache.lucene.document.Document;
import org.apache.lucene.index.LeafReader;
import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.search.*;
import org.apache.lucene.util.BitSet;
import org.apache.lucene.util.BitSetIterator;
import org.apache.lucene.util.FixedBitSet;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;


public class BibleRangeWeight extends ConstantScoreWeight {
    private final static float NOT_RELEVANT = Float.NEGATIVE_INFINITY;
    private final static Logger logger = LoggerFactory.getLogger(BibleRangeWeight.class);
    private final static String FIELD = "bible_ranges";
    private final IntFloatMap scoring = new IntFloatHashMap();
    private final boolean isSearchingForBooks;
    private final BibleRange[] ranges;
    private final Weight weight;

    public BibleRangeWeight(final BibleRangeQuery query, final BibleRange[] ranges, final Weight weight) {
        super(query);
        this.ranges = ranges;
        this.weight = weight;
        boolean isSearchingForBooks = false;
        for (final BibleRange range : ranges) {
            isSearchingForBooks |= range.isEntireBook();
        }
        this.isSearchingForBooks = isSearchingForBooks;
    }

    private boolean matches(final Document document) {
        final String dbField = document.get(FIELD);
        final BibleRange[] documentRanges = BibleRangeParser.getRangesFromDatabaseField(dbField);
        final BibleRange[] fieldRanges = isSearchingForBooks ? documentRanges : BibleRange.removeBooks(documentRanges);
        return fieldRanges.length != 0 && Range.hasIntersections(ranges, fieldRanges);
    }

    private float customScore(final Document doc) {
        final String dbField = doc.get(FIELD);
        if (dbField == null || dbField.isEmpty()) {
            return NOT_RELEVANT;
        }
        final Range[] field_ranges = BibleRangeParser.getRangesFromDatabaseField(dbField);
        return Range.getMatchingScore(field_ranges, ranges);
    }

    private BitSet getMatchingDocs(final DocIdSetIterator iterator, final LeafReaderContext leafReaderContext) throws IOException {
        final LeafReader reader = leafReaderContext.reader();
        final BitSet matchingDocs = new FixedBitSet(reader.maxDoc());
        for (int i = iterator.nextDoc(); i != DocIdSetIterator.NO_MORE_DOCS; i = iterator.nextDoc()) {
            final Document doc = reader.document(i);
            if (matches(doc)) {
                matchingDocs.set(i);
                scoring.put(i, customScore(doc));
            }
        }
        int index = matchingDocs.nextSetBit(0);
        if (index >= 0 && index < matchingDocs.length()) {
            return matchingDocs;
        }
        return null;
    }

    @Override
    public final Scorer scorer(LeafReaderContext context) throws IOException {
        if (this.weight == null) {
            return null;
        }
        final Scorer scorer = this.weight.scorer(context);
        if (scorer == null) {
            return null;
        }
        final DocIdSetIterator iterator = scorer.iterator();
        final BitSet matchingDocs = this.getMatchingDocs(iterator, context);
        if (matchingDocs != null) {
            final DocIdSetIterator approximation = new BitSetIterator(matchingDocs, matchingDocs.length());
            TwoPhaseIterator twoPhase = new TwoPhaseIterator(approximation) {
                public boolean matches() throws IOException {
                    int doc = this.approximation.docID();
                    return matchingDocs.get(doc);
                }

                public float matchCost() {
                    return 10.0F;
                }
            };
            return new BibleRangeScorer(this, scoring, twoPhase);
        } else {
            return null;
        }
    }
}
