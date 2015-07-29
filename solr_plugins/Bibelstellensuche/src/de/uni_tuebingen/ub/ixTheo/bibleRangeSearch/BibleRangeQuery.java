package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import java.io.IOException;

import org.apache.lucene.index.AtomicReaderContext;
import org.apache.lucene.queries.CustomScoreProvider;
import org.apache.lucene.queries.CustomScoreQuery;
import org.apache.lucene.search.Query;

public class BibleRangeQuery extends CustomScoreQuery {
	private final Range[] ranges;

	public BibleRangeQuery(Query subQuery, Range[] ranges) {
		super(subQuery);
		this.ranges = ranges;
	}

	@Override
	protected CustomScoreProvider getCustomScoreProvider(
			AtomicReaderContext context) throws IOException {
		return new BibleRangeScoreProvider(ranges, context);
	}
	
	@Override
	public String name() {
		return "Bible Reference Query";
	}
}
