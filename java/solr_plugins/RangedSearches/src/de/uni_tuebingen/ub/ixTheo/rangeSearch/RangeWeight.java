package de.uni_tuebingen.ub.ixTheo.rangeSearch;


import java.io.IOException;
import com.carrotsearch.hppc.IntFloatHashMap;
import com.carrotsearch.hppc.IntFloatMap;
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


public abstract class RangeWeight extends ConstantScoreWeight {
    protected final static float NOT_RELEVANT = Float.NEGATIVE_INFINITY;
    protected final Range[] ranges;
    protected final Weight weight;
    protected final IntFloatMap scoring = new IntFloatHashMap();

    public RangeWeight(final RangeQuery query, final Range[] ranges, final Weight weight) {
        super(query, 1.0F);
        this.ranges = ranges;
        this.weight = weight;
    }

    abstract protected boolean matches(final Document document);
    abstract protected float customScore(final Document doc);

    protected BitSet getMatchingDocuments(final DocIdSetIterator iterator, final LeafReaderContext leafReaderContext) throws IOException {
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
        final BitSet matchingDocs = this.getMatchingDocuments(iterator, context);
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
}
