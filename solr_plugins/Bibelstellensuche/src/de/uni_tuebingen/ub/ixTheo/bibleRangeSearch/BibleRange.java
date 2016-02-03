package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


public class BibleRange extends Range {

    public BibleRange(final String range) {
        super(getLower(range), getUpper(range));
    }

    private static int getLower(final String range) {
        if (range.length() == 15) {
            return Integer.valueOf(range.substring(0, 7));
        } else {
            return 0;
        }
    }

    private static int getUpper(final String range) {
        if (range.length() == 15) {
            return Integer.valueOf(range.substring(8, 15));
        } else {
            return 9999999;
        }
    }

    public static Range[] getRanges(final String[] ranges) {
        final Range[] queryRanges = new Range[ranges.length];
        for (int i = 0; i < queryRanges.length; i++) {
            queryRanges[i] = new BibleRange(ranges[i]);
        }
        return queryRanges;
    }

    public static Range[] getRanges(final String input, String separator) {
        final String[] fields = input.split(separator);
        return getRanges(fields);
    }
}
