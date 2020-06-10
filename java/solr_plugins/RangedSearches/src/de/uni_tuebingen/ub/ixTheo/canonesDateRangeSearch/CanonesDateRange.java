package de.uni_tuebingen.ub.ixTheo.canonesDateRangeSearch;


import java.util.ArrayList;
import java.util.List;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;


public class CanonesDateRange extends Range {
    final static int CANON_LAW_CODE_LENGTH = 9;

    public CanonesDateRange(final String date_range) {
        super(date_range);
    }

    public static CanonesDateRange[] getRanges(final String[] ranges) {
        final CanonesDateRange[] queryRanges = new CanonesDateRange[ranges.length];
        for (int i = 0; i < queryRanges.length; i++) {
            queryRanges[i] = new CanonesDateRange(ranges[i]);
        }
        return queryRanges;
    }

    public static CanonesDateRange[] getRanges(final String input, String separator) {
        final String[] fields = input.split(separator);
        return getRanges(fields);
    }

    public static CanonesDateRange[] removeCodices(CanonesDateRange[] ranges) {
        List<CanonesDateRange> filteredRanges = new ArrayList<>(ranges.length);
        for (final CanonesDateRange range : ranges) {
            if (!range.isEntireCodex()) {
                filteredRanges.add(range);
            }
        }
        return filteredRanges.toArray(new CanonesDateRange[filteredRanges.size()]);
    }

    public boolean isEntireCodex() {
        final long lower = getLower();
        final long upper = getUpper();
        return (lower == 100000000 && upper == 199999999) || (lower == 200000000 && upper == 299999999)
               || (lower == 300000000 && upper == 399999999);
    }
}
