package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

/** \class Range Represents a bible range. */
class Range {
	public final static int OUT_OF_RANGE = Integer.MAX_VALUE;

	private final int lower;
	private final int upper;

	public static int getDistance(final Range[] field_ranges,
			Range[] query_ranges) {
		int distance = 0;
		for (final Range query_range : query_ranges) {
			int query_distance = query_range.getMinimumDistance(field_ranges);
			if (query_distance == OUT_OF_RANGE) {
				return OUT_OF_RANGE;
			}
			distance += query_distance;
		}

		return distance;
	}

	public static boolean doAllSourceRangesIntersectsSomeTargetRanges(
			final Range[] source_ranges, final Range[] target_ranges) {
		loop: for (final Range source_range : source_ranges) {
			for (final Range target_range : target_ranges) {
				if (source_range.intersects(target_range)) {
					continue loop;
				}
			}
			return false;
		}
		return true;
	}
	
	public Range(int lower, int upper) {
		this.lower = lower;
		this.upper = upper;
	}

	public final int getLower() {
		return lower;
	}

	public final int getUpper() {
		return upper;
	}

	public int getDistance(final Range other) {
		if (!intersects(other)) {
			return OUT_OF_RANGE;
		}
		return Math.abs(lower - other.lower) + Math.abs(other.upper - upper);
	}

	public boolean intersects(Range other) {
		return other.upper >= lower && other.lower <= upper;
	}

	public int getMinimumDistance(Range[] targets) {
		int query_distance = OUT_OF_RANGE;
		for (final Range target : targets) {
			query_distance = Math.min(getDistance(target), query_distance);
		}
		return query_distance;
	}

}
