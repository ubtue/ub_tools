package de.uni_tuebingen.ub.ixTheo.multiLanguageQuery;

import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.SolrException;
import org.apache.solr.common.SolrException.ErrorCode;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.QParserPlugin;


public class MultiLanguageQueryParserPlugin extends QParserPlugin{

    @Override
    @SuppressWarnings("rawtypes")
    public void init(NamedList args) {
    }

    @Override
    public QParser createParser(String queryString, SolrParams localParams, SolrParams params, SolrQueryRequest req) {
        try {
            return new MultiLanguageQueryParser(queryString, localParams, params, req);
        } catch (MultiLanguageQueryParserException e) {
           throw new SolrException(ErrorCode.SERVER_ERROR, "Error instantiating MultiLanguageQueryParser", e);
        }
    }
}
