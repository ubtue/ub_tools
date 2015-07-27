package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import org.apache.solr.common.params.SolrParams;
import org.apache.solr.common.util.NamedList;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.QParserPlugin;

public class BibleRangeParserPlugin extends QParserPlugin {

	@Override
	@SuppressWarnings("rawtypes")
	public void init(NamedList args) {
	}

	@Override
	public QParser createParser(String qstr, SolrParams localParams, SolrParams params,
			SolrQueryRequest req) {
		return new BibleRangeParser(qstr, localParams, params, req);
	}

}
