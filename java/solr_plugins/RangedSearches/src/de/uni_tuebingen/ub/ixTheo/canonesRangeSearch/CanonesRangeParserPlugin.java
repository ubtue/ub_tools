package de.uni_tuebingen.ub.ixTheo.canonesRangeSearch;


import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeParserPlugin;


public class CanonesRangeParserPlugin extends RangeParserPlugin {

    @Override
    public QParser createParser(final String queryString, final SolrParams localParams, final SolrParams params, final SolrQueryRequest req) {
        return new CanonesRangeParser(queryString, localParams, params, req);
    }
}
