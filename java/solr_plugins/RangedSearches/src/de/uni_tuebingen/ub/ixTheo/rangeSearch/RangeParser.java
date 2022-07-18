package de.uni_tuebingen.ub.ixTheo.rangeSearch;


import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;


abstract public class RangeParser extends QParser {
    protected final static String DB_FIELD_SEPARATOR = ",";
    protected final static String QUERY_FIELD_SEPARATOR = " ";
    protected final static String QUERY_OR_SEPARATOR = " OR ";

    public RangeParser(String qstr, SolrParams localParams, SolrParams params, SolrQueryRequest req) {
        super(qstr, localParams, params, req);
    }
}
