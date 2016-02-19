package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import java.util.ArrayList;
import java.util.List;


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

    public static BibleRange[] getRanges(final String[] ranges) {
        final BibleRange[] queryRanges = new BibleRange[ranges.length];
        for (int i = 0; i < queryRanges.length; i++) {
            queryRanges[i] = new BibleRange(ranges[i]);
        }
        return queryRanges;
    }

    public static BibleRange[] getRanges(final String input, String separator) {
        final String[] fields = input.split(separator);
        return getRanges(fields);
    }

    public static BibleRange[] removeBooks(BibleRange[] ranges) {
        List<BibleRange> filteredRanges = new ArrayList<>(ranges.length);
        for (final BibleRange range : ranges) {
            if (!range.isBook()) {
                filteredRanges.add(range);
            }
        }
        return filteredRanges.toArray(new BibleRange[filteredRanges.size()]);
    }

    public boolean isVerse() {
        return !isChepter();
    }

    public boolean isChepter() {
        return (getLower() % 100) == 0 && (getUpper() % 100) == 99 && !isBook();
    }

    public boolean isBook() {
        return (getLower() % 100000) == 0 && (getUpper() % 100000) == 99999;
    }

}
