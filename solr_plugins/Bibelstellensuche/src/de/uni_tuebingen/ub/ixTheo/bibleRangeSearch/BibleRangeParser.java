package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import org.apache.lucene.search.FilteredQuery;
import org.apache.lucene.search.Query;
import org.apache.solr.common.params.SolrParams;
import org.apache.solr.request.SolrQueryRequest;
import org.apache.solr.search.QParser;
import org.apache.solr.search.SyntaxError;

public class BibleRangeParser extends QParser {
	private final static String QUERY_SEPERATOR = " ";
	private final static String DB_FIELD_SEPERATOR = ",";

	private Query innerQuery;

	public BibleRangeParser(String qstr, SolrParams localParams,
			SolrParams params, SolrQueryRequest req) {
		super(qstr, localParams, params, req);
		try {
			QParser parser = getParser("+bible_ranges:*", "lucene", getReq());
			Range[] ranges = getRangesFromQuery();
			this.innerQuery = new FilteredQuery(new BibleRangeQuery(
					parser.parse(), ranges), new BibleRangeFilter(ranges));
		} catch (SyntaxError ex) {
			throw new RuntimeException("error parsing query", ex);
		}
	}

	@Override
	public Query parse() throws SyntaxError {
		return this.innerQuery;
	}

	private Range[] getRangesFromQuery() {
		return BibleRange.getRanges(getFieldsFromQuery());
	}

	private String[] getFieldsFromQuery() {
		return qstr.split(QUERY_SEPERATOR);
	}

	public static Range[] getRangesFromDatabaseField(final String db_field) {
		return BibleRange.getRanges(db_field, DB_FIELD_SEPERATOR);
	}
}
