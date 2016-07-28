package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import java.util.ArrayList;
import java.util.List;


public class BibleRange extends Range {
    private final static int VERSE_LENGTH = 3;
    private final static int CHAPTER_LENGTH = 3;
    private final static int BOOK_LENGTH = 2;

    private final static int CHAPTER_MASK = tenToThePowerOf(VERSE_LENGTH + CHAPTER_LENGTH);
    private final static int BOOK_MASK = tenToThePowerOf(VERSE_LENGTH + CHAPTER_LENGTH + BOOK_LENGTH);

    private final static int MAX_CHAPTER_CODE = CHAPTER_MASK - 1;
    private final static int MAX_BOOK_CODE = BOOK_MASK - 1;

    private final static int BIBLE_CODE_LENGTH = VERSE_LENGTH + CHAPTER_LENGTH + BOOK_LENGTH;

    private final static int LOWER_BIBLE_CODE_START = 0;
    private final static int UPPER_BIBLE_CODE_START = LOWER_BIBLE_CODE_START + BIBLE_CODE_LENGTH + 1;

    private final static int RANGE_STRING_LENGTH = BIBLE_CODE_LENGTH + 1 + BIBLE_CODE_LENGTH;

    public BibleRange(final String range) {
        super(getLower(range), getUpper(range));
    }

    private static int getLower(final String range) {
        if (range.length() == RANGE_STRING_LENGTH) {
            return Integer.valueOf(range.substring(LOWER_BIBLE_CODE_START, LOWER_BIBLE_CODE_START + BIBLE_CODE_LENGTH));
        } else {
            return 0;
        }
    }

    private static int getUpper(final String range) {
        if (range.length() == RANGE_STRING_LENGTH) {
            return Integer.valueOf(range.substring(UPPER_BIBLE_CODE_START, UPPER_BIBLE_CODE_START + BIBLE_CODE_LENGTH));
        } else {
            return MAX_BOOK_CODE;
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
            if (!range.isEntireBook()) {
                filteredRanges.add(range);
            }
        }
        return filteredRanges.toArray(new BibleRange[filteredRanges.size()]);
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
