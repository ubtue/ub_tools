package de.uni_tuebingen.ub.ixTheo.timeAspectRangeSearch;


import java.io.IOException;

import org.apache.lucene.document.Document;
import org.apache.lucene.index.LeafReader;
import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.search.ConstantScoreWeight;
import org.apache.lucene.search.DocIdSetIterator;
import org.apache.lucene.search.Scorer;
import org.apache.lucene.search.TwoPhaseIterator;
import org.apache.lucene.search.Weight;
import org.apache.lucene.util.BitSet;
import org.apache.lucene.util.BitSetIterator;
import org.apache.lucene.util.FixedBitSet;

import com.carrotsearch.hppc.IntFloatHashMap;
import com.carrotsearch.hppc.IntFloatMap;

import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeScorer;


public class TimeAspectRangeWeight extends ConstantScoreWeight {
    private final static float NOT_RELEVANT = Float.NEGATIVE_INFINITY;
    //private final static Logger logger = LoggerFactory.getLogger(TimeAspectRangeWeight.class);
    private final static String FIELD = "time_aspect_ranges";
    private final IntFloatMap scoring = new IntFloatHashMap();
    private final TimeAspectRange[] ranges;
    private final Weight weight;

    public TimeAspectRangeWeight(final TimeAspectRangeQuery query, final TimeAspectRange[] ranges, final Weight weight) {
        super(query, 1.0F);
        this.ranges = ranges;
        this.weight = weight;
    }

    private boolean matches(final Document document) {
        final String dbField = document.get(FIELD);
        final TimeAspectRange[] documentRanges = TimeAspectRangeParser.getRangesFromDatabaseField(dbField);
        return documentRanges.length != 0 && Range.hasIntersections(ranges, documentRanges);
    }

    private float customScore(final Document doc) {
        final String dbField = doc.get(FIELD);
        if (dbField == null || dbField.isEmpty()) {
            return NOT_RELEVANT;
        }
        final Range[] field_ranges = TimeAspectRangeParser.getRangesFromDatabaseField(dbField);
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
            return new RangeScorer(this, scoring, twoPhase);
        } else {
            return null;
        }
    }

    @Override
    public final boolean isCacheable(LeafReaderContext context) {
        return false;
    }
}
