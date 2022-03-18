package de.uni_tuebingen.ub.ixTheo.rangeSearch;


import com.carrotsearch.hppc.IntFloatMap;
import org.apache.lucene.search.ConstantScoreScorer;
import org.apache.lucene.search.DocIdSetIterator;
import org.apache.lucene.search.Scorer;
import org.apache.lucene.search.ScoreMode;
import org.apache.lucene.search.TwoPhaseIterator;
import org.apache.lucene.search.Weight;

import java.io.IOException;


public class RangeScorer extends Scorer {
    private final TwoPhaseIterator twoPhaseIterator;
    private final DocIdSetIterator disi;
    private final IntFloatMap scoring;
    private final ConstantScoreScorer scorer;

    public RangeScorer(final Weight weight, final IntFloatMap scoring, final TwoPhaseIterator twoPhaseIterator) {
        super(weight);
        this.twoPhaseIterator = twoPhaseIterator;
        this.scoring = scoring;
        this.disi = TwoPhaseIterator.asDocIdSetIterator(twoPhaseIterator);
        this.scorer = new ConstantScoreScorer(weight, scoreSave(), ScoreMode.COMPLETE, this.disi);
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

    private float scoreSave(){
        try {
            return scoring.get(docID());
        }
        catch (Exception e) {
            return 1.0f;
        }
    }

    public int freq() throws IOException {
        return 1;
    }

    @Override
    public float getMaxScore(int upTo) throws IOException {
        return scorer.getMaxScore(upTo);
    }
}
