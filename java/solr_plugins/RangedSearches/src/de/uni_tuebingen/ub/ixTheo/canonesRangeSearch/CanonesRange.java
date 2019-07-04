package de.uni_tuebingen.ub.ixTheo.canonesRangeSearch;


import java.util.ArrayList;
import java.util.List;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;


public class CanonesRange extends Range {
    public CanonesRange(final String range) {
        super(getLower(range), getUpper(range));
    }

    private static int getLower(final String range) {
        if (range.length() == 9 + 1 + 9) {
            return Integer.valueOf(range.substring(0, 9));
        } else {
            return 0;
        }
    }

    private static int getUpper(final String range) {
        if (range.length() == 9 + 1 + 9) {
            return Integer.valueOf(range.substring(9 + 1, 9 + 1 + 9));
        } else {
            return 999999999;
        }
    }

    public static CanonesRange[] getRanges(final String[] ranges) {
        final CanonesRange[] queryRanges = new CanonesRange[ranges.length];
        for (int i = 0; i < queryRanges.length; i++) {
            queryRanges[i] = new CanonesRange(ranges[i]);
        }
        return queryRanges;
    }

    public static CanonesRange[] getRanges(final String input, String separator) {
        final String[] fields = input.split(separator);
        return getRanges(fields);
    }

    public static CanonesRange[] removeCodices(CanonesRange[] ranges) {
        List<CanonesRange> filteredRanges = new ArrayList<>(ranges.length);
        for (final CanonesRange range : ranges) {
            if (!range.isEntireCodex()) {
                filteredRanges.add(range);
            }
        }
        return filteredRanges.toArray(new CanonesRange[filteredRanges.size()]);
    }

    public boolean isEntireCodex() {
        final int lower = getLower();
        final int upper = getUpper();
        return (lower == 100000000 && upper == 199999999) || (lower == 200000000 && upper == 299999999)
               || (lower == 300000000 && upper == 399999999);
    }
}
