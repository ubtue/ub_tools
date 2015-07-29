package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

/** \class Range Represents a bible range. */
class Range {
	public final static int OUT_OF_RANGE = Integer.MAX_VALUE;

	private final int lower;
	private final int upper;

	public static int getDistance(final Range[] fieldRanges, final Range[] queryRanges) {
		int distance = 0;
		for (final Range queryRange : queryRanges) {
			final int queryDistance = queryRange.getMinimumDistance(fieldRanges);
			if (queryDistance == OUT_OF_RANGE) {
				return OUT_OF_RANGE;
			}
			distance += queryDistance;
		}

		return distance;
	}

	public static boolean doAllSourceRangesIntersectsSomeTargetRanges(final Range[] sourceRanges, final Range[] targetRanges) {
		loop: for (final Range sourceRange : sourceRanges) {
			for (final Range targetRange : targetRanges) {
				if (sourceRange.intersects(targetRange)) {
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
		int queryDistance = OUT_OF_RANGE;
		for (final Range target : targets) {
			queryDistance = Math.min(getDistance(target), queryDistance);
		}
		return queryDistance;
	}
}
