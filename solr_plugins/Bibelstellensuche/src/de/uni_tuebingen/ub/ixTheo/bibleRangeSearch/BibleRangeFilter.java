package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import java.io.IOException;

import org.apache.lucene.index.AtomicReaderContext;
import org.apache.lucene.index.BinaryDocValues;
import org.apache.lucene.search.DocIdSet;
import org.apache.lucene.search.FieldCache;
import org.apache.lucene.search.FieldCacheDocIdSet;
import org.apache.lucene.search.Filter;
import org.apache.lucene.util.Bits;
import org.apache.lucene.util.BytesRef;

public class BibleRangeFilter extends Filter {

	private String field = "bible_ranges";
	private final Range[] ranges;

	public BibleRangeFilter(Range[] ranges) {
		this.ranges = ranges;
	}

	public FieldCache getFieldCache() {
		return FieldCache.DEFAULT;
	}

	@Override
	public DocIdSet getDocIdSet(AtomicReaderContext context, Bits acceptDocs)
			throws IOException {
		final BinaryDocValues values = FieldCache.DEFAULT.getTerms(context.reader(), field);
		return new FastDocIdSet(context.reader().maxDoc(), acceptDocs, values);
	}
	
	class FastDocIdSet extends FieldCacheDocIdSet {
		private final BinaryDocValues values;
		public FastDocIdSet(int maxDoc, Bits acceptDocs, BinaryDocValues values) {
			super(maxDoc, acceptDocs);
			this.values = values;
		}
		
		@Override
		protected final boolean matchDoc(int doc_id) {
			BytesRef ref = new BytesRef();
			values.get(doc_id, ref);
			
			final String db_field = ref.utf8ToString();
			if (db_field == null || db_field.isEmpty()) {
				return false;
			}
			final Range[] field_ranges = BibleRangeParser
					.getRangesFromDatabaseField(db_field);
			return Range.doAllSourceRangesIntersectsSomeTargetRanges(
					ranges, field_ranges);
		}
	}
}
