package de.uni_tuebingen.ub.ixTheo.rangeSearch;

import java.beans.Expression;
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

    // This needs to be a function, since we cannot override class variables in Java
    // subclasses, only methods.
    abstract protected String getRangeFieldName();

    abstract protected Range[] getRangesFromDatabaseField(final String dbField);

    protected boolean matches(final Document document) {
        final String dbField = document.get(getRangeFieldName());
        final Range[] documentRanges = getRangesFromDatabaseField(dbField);
        return documentRanges.length != 0 && Range.hasIntersections(ranges, documentRanges);
    }

    protected float customScore(final Document document) {
        final String dbField = document.get(getRangeFieldName());
        if (dbField == null || dbField.isEmpty()) {
            return NOT_RELEVANT;
        }
        final Range[] field_ranges = getRangesFromDatabaseField(dbField);
        return Range.getMatchingScore(field_ranges, ranges);
    }

    // This is just a dummy implementation, override in subclass to get more
    // detailed explanations.
    // Add "debug=true" to a solr query to see the explanations in the response's
    // debug section.
    protected Explanation explain(final Document document) {
        if (matches(document)) {
            return Explanation.match(customScore(document), "match");
        } else {
            return Explanation.noMatch("no match");
        }
    }

    // this is a mandatory function to implement
    @Override
    public Explanation explain(LeafReaderContext context, int doc) throws IOException {
        return explain(context.reader().document(doc));
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

    @Override
    public final boolean isCacheable(LeafReaderContext context) {
        return false;
    }

    @Override
    @Deprecated
    public void extractTerms(Set<Term> terms) {
        super.extractTerms(terms);
    }

}
