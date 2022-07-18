package de.uni_tuebingen.ub.ixTheo.rangeSearch;


import java.io.IOException;
import java.util.Set;
import org.apache.lucene.document.Document;
import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.index.Term;
import org.apache.lucene.search.Explanation;
import org.apache.lucene.search.Scorer;
import org.apache.lucene.search.Weight;


public abstract class RangeWeight extends Weight {
    protected final static float NOT_RELEVANT = Float.NEGATIVE_INFINITY;
    protected final Range[] ranges;
    protected final Weight weight;

    public RangeWeight(final RangeQuery query, final Range[] ranges, final Weight weight) {
        super(query);
        this.ranges = ranges;
        this.weight = weight;
    }

    abstract protected boolean matches(final Document document);
    abstract protected float customScore(final Document doc);

    // This is just a dummy implementation, override in subclass to get more detailed explanations.
    // Add "debug=true" to a solr query to see the explanations in the response's debug section.
    protected Explanation explain(final Document document) {
        if (matches(document)) {
            return Explanation.match(customScore(document), "match");
        } else {
            return Explanation.noMatch("no match");
        }
    }

    @Override
    public Explanation explain(LeafReaderContext context, int documentId) throws IOException {
        return explain(context.reader().document(documentId));
    }

    @Override
    @Deprecated
    public void extractTerms(Set<Term> terms) {
        // This is just a dummy, the function is deprecated (but mandatory) in Solr 8.
    }

    @Override
    public final RangeScorer scorer(LeafReaderContext context) throws IOException {
        if (this.weight == null) {
            return null;
        }
        final Scorer scorer = this.weight.scorer(context);
        if (scorer == null) {
            return null;
        }
        return new RangeScorer(this, this.weight, context);
    }
}
