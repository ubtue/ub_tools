package de.uni_tuebingen.ub.ixTheo.rangeSearch;


import java.io.IOException;
import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.search.DocIdSetIterator;
import org.apache.lucene.search.FilteredDocIdSetIterator;
import org.apache.lucene.search.Scorer;
import org.apache.lucene.search.Weight;


public class RangeScorer extends Scorer {
    protected final DocIdSetIterator iterator;
    protected final LeafReaderContext context;
    protected final RangeWeight rangeWeight;
    protected final Weight parentWeight;

    public RangeScorer(final RangeWeight rangeWeight, final Weight parentWeight, final LeafReaderContext context) throws IOException {
        super(rangeWeight);
        this.context = context;
        this.rangeWeight = rangeWeight;
        this.parentWeight = parentWeight;

        iterator = new FilteredDocIdSetIterator(parentWeight.scorer(context).iterator()) {
            @Override
            protected boolean match(int doc) {
                try {
                    return rangeWeight.matches(context.reader().document(doc));
                } catch (IOException ex) {
                    return false;
                }
            }
        };
    }

    @Override
    public DocIdSetIterator iterator() {
        return iterator;
    }

    @Override
    public int docID() {
        return iterator().docID();
    }

    @Override
    public float score() throws IOException {
        return rangeWeight.customScore(context.reader().document(docID()));
    }

    @Override
    public float getMaxScore(int upTo) throws IOException {
        return parentWeight.scorer(context).getMaxScore(upTo);
    }
}
