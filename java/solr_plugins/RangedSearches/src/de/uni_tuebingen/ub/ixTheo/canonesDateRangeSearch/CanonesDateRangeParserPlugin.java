package de.uni_tuebingen.ub.ixTheo.canonesDateRangeSearch;


import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.RangeParserPlugin;


public class CanonesDateRangeParserPlugin extends RangeParserPlugin {

    @Override
    public QParser createParser(final String queryString, final SolrParams localParams, final SolrParams params, final SolrQueryRequest req) {
        return new CanonesDateRangeParser(queryString, localParams, params, req);
    }
}
