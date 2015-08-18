package de.uni_tuebingen.ub.ixTheo.keywordChainSearch;


import org.apache.lucene.search.FilteredQuery;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;

public class KeywordChainParserParser extends QParser {

    private Query innerQuery;

    public KeywordChainParserParser(final String searchString, final SolrParams localParams, final SolrParams params, final SolrQueryRequest request) {
        super(searchString, localParams, params, request);
        try {
	    final String queryString = "key_word_chain_bag:(" + searchString +")";
            final QParser parser = getParser(queryString, "lucene", getReq());
	    this.innerQuery = parser.parse();
        } catch (SyntaxError ex) {
            throw new RuntimeException("error parsing query", ex);
        }
    }


    public Query parse() throws SyntaxError {
        return this.innerQuery;
    }


}



