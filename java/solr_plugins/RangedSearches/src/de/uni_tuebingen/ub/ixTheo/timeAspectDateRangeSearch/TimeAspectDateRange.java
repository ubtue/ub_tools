package de.uni_tuebingen.ub.ixTheo.timeAspectDateRangeSearch;


import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;


public class TimeAspectDateRange extends Range {
    final static int TIME_ASPECT_CODE_LENGTH = 12;

    public TimeAspectDateRange(final String date_range) {
        super(date_range);
    }

    public static TimeAspectDateRange[] getRanges(final String[] date_ranges) {
        final TimeAspectDateRange[] queryRanges = new TimeAspectDateRange[date_ranges.length];
        for (int i = 0; i < queryRanges.length; ++i) {
            queryRanges[i] = new TimeAspectDateRange(date_ranges[i]);
        }
        return queryRanges;
    }

    public static TimeAspectDateRange[] getRanges(final String input, String separator) {
        final String[] fields = input.split(separator);
        return getRanges(fields);
    }
}
