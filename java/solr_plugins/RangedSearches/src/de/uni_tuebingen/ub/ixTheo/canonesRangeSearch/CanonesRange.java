package de.uni_tuebingen.ub.ixTheo.canonesRangeSearch;


import java.util.ArrayList;
import java.util.List;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;


public class CanonesRange extends Range {
    final static int CANON_LAW_CODE_LENGTH = 9;

    public CanonesRange(final String range) {
        super(getLower(range), getUpper(range));
    }

    private static long getLower(final String range) {
        if (range.length() == CANON_LAW_CODE_LENGTH + 1 + CANON_LAW_CODE_LENGTH) {
            return Long.valueOf(range.substring(0, CANON_LAW_CODE_LENGTH));
        } else {
            return 0;
        }
    }

    private static long getUpper(final String range) {
        if (range.length() == CANON_LAW_CODE_LENGTH + 1 + CANON_LAW_CODE_LENGTH) {
            return Long.valueOf(range.substring(CANON_LAW_CODE_LENGTH + 1, CANON_LAW_CODE_LENGTH + 1 + CANON_LAW_CODE_LENGTH));
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
        final long lower = getLower();
        final long upper = getUpper();
        return (lower == 100000000 && upper == 199999999) || (lower == 200000000 && upper == 299999999)
               || (lower == 300000000 && upper == 399999999);
    }
}
