package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import com.carrotsearch.hppc.IntFloatMap;
import org.apache.lucene.search.DocIdSetIterator;
import org.apache.lucene.search.Scorer;
import org.apache.lucene.search.TwoPhaseIterator;
import org.apache.lucene.search.Weight;

import java.io.IOException;


class BibleRangeScorer extends Scorer {
    private final TwoPhaseIterator twoPhaseIterator;
    private final DocIdSetIterator disi;
    private final IntFloatMap scoring;

    BibleRangeScorer(final Weight weight, final IntFloatMap scoring, final TwoPhaseIterator twoPhaseIterator) {
        super(weight);
        this.twoPhaseIterator = twoPhaseIterator;
        this.scoring = scoring;
        this.disi = TwoPhaseIterator.asDocIdSetIterator(twoPhaseIterator);
    }

    public DocIdSetIterator iterator() {
        return this.disi;
    }

    public TwoPhaseIterator twoPhaseIterator() {
        return this.twoPhaseIterator;
    }

    public int docID() {
        return this.disi.docID();
    }

    public float score() throws IOException {
        return scoring.get(docID());
    }

    public int freq() throws IOException {
        return 1;
    }
}
