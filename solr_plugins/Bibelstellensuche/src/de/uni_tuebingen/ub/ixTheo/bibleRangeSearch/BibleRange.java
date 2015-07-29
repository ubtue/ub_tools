package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

public class BibleRange extends Range {

	public BibleRange(final String range) {
		super(getLower(range), getUpper(range));
	}

	private static int getLower(String range) {
		if (range.length() == 15) {
			return Integer.valueOf(range.substring(0, 7));
		} else {
			return 0;
		}
	}

	private static int getUpper(String range) {
		if (range.length() == 15) {
			return Integer.valueOf(range.substring(9, 15));
		} else {
			return 9999999;
		}
	}

	public static Range[] getRanges(String[] ranges) {
		final Range[] query_ranges = new Range[ranges.length];
		for (int i = 0; i < query_ranges.length; i++) {
			query_ranges[i] = new BibleRange(ranges[i]);
		}
		return query_ranges;
	}

	public static Range[] getRanges(String input, String seperator) {
		final String[] fields = input.split(seperator);
		return getRanges(fields);
	}
}
