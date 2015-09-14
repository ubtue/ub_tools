package de.uni_tuebingen.ub.ixTheo.keywordChainSearch;

import java.io.IOException;


import org.apache.lucene.index.AtomicReaderContext;
import org.apache.lucene.queries.CustomScoreProvider;
import org.apache.lucene.queries.CustomScoreQuery;
import org.apache.lucene.search.Query;

public class KeywordChainSearchQuery extends CustomScoreQuery {
      
        final String queryString;

        public KeywordChainSearchQuery(final Query subQuery) {
                super(subQuery);
		queryString = subQuery.toString();
        }

        @Override
        protected CustomScoreProvider getCustomScoreProvider(final AtomicReaderContext context) throws IOException {
	    return new KeywordChainScoreProvider(context, queryString);
        }
        
        @Override
        public String name() {
                return "Keyword Chain Search Query";
        }
}
