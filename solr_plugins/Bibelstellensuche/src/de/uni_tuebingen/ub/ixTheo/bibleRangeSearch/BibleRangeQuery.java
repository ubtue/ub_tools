package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import org.apache.lucene.index.AtomicReaderContext;
import org.apache.lucene.queries.CustomScoreProvider;
import org.apache.lucene.queries.CustomScoreQuery;
import org.apache.lucene.search.Query;

import java.io.IOException;


public class BibleRangeQuery extends CustomScoreQuery {
    private final Range[] ranges;

    public BibleRangeQuery(final Query subQuery, final Range[] ranges) {
        super(subQuery);
        this.ranges = ranges;
    }

    @Override
    protected CustomScoreProvider getCustomScoreProvider(final AtomicReaderContext context) throws IOException {
        return new BibleRangeScoreProvider(ranges, context);
    }

    @Override
    public String name() {
        return "Bible Reference Query";
    }
}
