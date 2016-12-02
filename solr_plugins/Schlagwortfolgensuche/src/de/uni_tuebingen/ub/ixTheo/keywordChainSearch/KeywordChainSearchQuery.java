package de.uni_tuebingen.ub.ixTheo.keywordChainSearch;


import org.apache.lucene.index.LeafReaderContext;
import org.apache.lucene.queries.CustomScoreProvider;
import org.apache.lucene.queries.CustomScoreQuery;
import org.apache.lucene.search.Query;

import java.io.IOException;


public class KeywordChainSearchQuery extends CustomScoreQuery {

    final Query query;
    final String origQueryString;

    public KeywordChainSearchQuery(final Query subQuery, final String origQueryString) {
        super(subQuery);
        this.query = subQuery;
        this.origQueryString = origQueryString;
    }

    @Override
    protected CustomScoreProvider getCustomScoreProvider(final LeafReaderContext context) throws IOException {
        return new KeywordChainScoreProvider(context, query, origQueryString);
    }

    @Override
    public String name() {
        return "Keyword Chain Search Query";
    }
}
