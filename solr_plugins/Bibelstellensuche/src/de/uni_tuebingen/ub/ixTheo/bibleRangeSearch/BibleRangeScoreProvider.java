package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import java.io.IOException;

import org.apache.lucene.document.Document;
import org.apache.lucene.index.AtomicReaderContext;
import org.apache.lucene.index.IndexReader;
import org.apache.lucene.queries.CustomScoreProvider;

public class BibleRangeScoreProvider extends CustomScoreProvider {
	private final static float BEST_SCORE = Float.POSITIVE_INFINITY;
	private final static float NOT_RELEVANT = Float.NEGATIVE_INFINITY;

	private final Range[] ranges;

	public BibleRangeScoreProvider(final Range[] ranges,
			final AtomicReaderContext context) {
		super(context);
		this.ranges = ranges;
	}

	@Override
	public float customScore(final int doc_id, float subQueryScore,
			float valSrcScores[]) throws IOException {
		final IndexReader index_reader = this.context.reader();
		final Document doc = index_reader.document(doc_id);
		final String db_field = doc.get("bible_ranges");

		if (db_field == null || db_field.isEmpty()) {
			return NOT_RELEVANT;
		}

		final Range[] field_ranges = BibleRangeParser.getRangesFromDatabaseField(db_field);
		final int distance = Range.getDistance(field_ranges, ranges);
		return (distance == 0) ? BEST_SCORE : (1.0f / (float) distance);
	}
}
