package de.uni_tuebingen.ub.ixTheo.bibleDateRangeSearch;


import java.util.ArrayList;
import java.util.List;
import de.uni_tuebingen.ub.ixTheo.rangeSearch.Range;


public class BibleDateRange extends Range {
    private final static int VERSE_LENGTH = 3;
    private final static int CHAPTER_LENGTH = 3;
    //private final static int BOOK_LENGTH = 2;

    private final static int CHAPTER_MASK = tenToThePowerOf(VERSE_LENGTH + CHAPTER_LENGTH);
    //private final static int BOOK_MASK = tenToThePowerOf(VERSE_LENGTH + CHAPTER_LENGTH + BOOK_LENGTH);

    private final static int MAX_CHAPTER_CODE = CHAPTER_MASK - 1;
    //private final static int MAX_BOOK_CODE = BOOK_MASK - 1;

    //private final static int BIBLE_CODE_LENGTH = VERSE_LENGTH + CHAPTER_LENGTH + BOOK_LENGTH;

    //private final static int LOWER_BIBLE_CODE_START = 0;
    //private final static int UPPER_BIBLE_CODE_START = LOWER_BIBLE_CODE_START + BIBLE_CODE_LENGTH + 1;

    //private final static int RANGE_STRING_LENGTH = BIBLE_CODE_LENGTH + 1 + BIBLE_CODE_LENGTH;

    public BibleDateRange(final String date_range) {
        super(date_range);
    }

    public static BibleDateRange[] getRanges(final String[] ranges) {
        final BibleDateRange[] queryRanges = new BibleDateRange[ranges.length];
        for (int i = 0; i < queryRanges.length; i++) {
            queryRanges[i] = new BibleDateRange(ranges[i]);
        }
        return queryRanges;
    }

    public static BibleDateRange[] getRanges(final String input, String separator) {
        final String[] fields = input.split(separator);
        return getRanges(fields);
    }

    public static BibleDateRange[] removeBooks(BibleDateRange[] ranges) {
        List<BibleDateRange> filteredRanges = new ArrayList<>(ranges.length);
        for (final BibleDateRange range : ranges) {
            if (!range.isEntireBook()) {
                filteredRanges.add(range);
            }
        }
        return filteredRanges.toArray(new BibleDateRange[filteredRanges.size()]);
    }

    private static int tenToThePowerOf(int exp) {
        int base = 10;
        for (; exp > 0; --exp) {
            base *= 10;
        }
        return base;
    }

    public boolean isEntireBook() {
        return (getLower() % CHAPTER_MASK) == 0 && (getUpper() % CHAPTER_MASK) == MAX_CHAPTER_CODE;
    }
}
