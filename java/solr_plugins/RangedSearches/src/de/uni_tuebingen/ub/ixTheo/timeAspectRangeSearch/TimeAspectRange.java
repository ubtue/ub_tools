package de.uni_tuebingen.ub.ixTheo.timeAspectRangeSearch;


import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;


public class TimeAspectRange extends Range {
    final static int TIME_ASPECT_CODE_LENGTH = 12;

    public TimeAspectRange(final String range) {
        super(getLower(range), getUpper(range));
    }

    private static long getLower(final String range) {
        if (range.length() == TIME_ASPECT_CODE_LENGTH + 1 + TIME_ASPECT_CODE_LENGTH) {
            return Long.valueOf(range.substring(0, TIME_ASPECT_CODE_LENGTH));
        } else {
            return 0;
        }
    }

    private static long getUpper(final String range) {
        if (range.length() == TIME_ASPECT_CODE_LENGTH + 1 + TIME_ASPECT_CODE_LENGTH) {
            return Long.valueOf(range.substring(TIME_ASPECT_CODE_LENGTH + 1, TIME_ASPECT_CODE_LENGTH + 1 + TIME_ASPECT_CODE_LENGTH));
        } else {
            return 999999999999L;
        }
    }

    public static TimeAspectRange[] getRanges(final String[] ranges) {
        final TimeAspectRange[] queryRanges = new TimeAspectRange[ranges.length];
        for (int i = 0; i < queryRanges.length; ++i) {
            queryRanges[i] = new TimeAspectRange(ranges[i]);
        }
        return queryRanges;
    }

    public static TimeAspectRange[] getRanges(final String input, String separator) {
        final String[] fields = input.split(separator);
        return getRanges(fields);
    }
}
