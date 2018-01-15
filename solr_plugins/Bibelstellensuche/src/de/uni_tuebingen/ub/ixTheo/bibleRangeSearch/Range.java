package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import java.util.Arrays;

/**
 * \class Range represents a range in a hierarchy of ranges.  An example would be parts of the Bible.
 */
class Range {
    private int lower;
    private int upper;

    private static float getRangesScore(final Range[] ranges, final Range[] queryRanges) {
        float best_individual_distance = Float.NEGATIVE_INFINITY;
        for (final Range range : ranges) {
            float distance = range.getBestMatchingScore(queryRanges);
            if (distance > best_individual_distance)
                best_individual_distance = distance;
        }
        return best_individual_distance;
    }

    public static float getMatchingScore(final Range[] fieldRanges, final Range[] queryRanges) {
        final Range[] mergedFieldRanges = Range.merge(fieldRanges);
        return Math.max(getRangesScore(fieldRanges, queryRanges), getRangesScore(mergedFieldRanges, queryRanges));
    }

    // N.B. This function assumes that the lower ends of "sourceRanges", and similarly for "targetRanges",
    // are monotonically increasing.
    public static boolean hasIntersections(final Range[] sourceRanges, final Range[] targetRanges) {
        for (final Range sourceRange : sourceRanges) {
            for (final Range targetRange : targetRanges) {
                if (sourceRange.intersects(targetRange)) {
                    return true;
                }
            }
        }
        return false;
    }

    // N.B. This function assumes that the lower ends of the input ranges "ranges" are monotonically increasing.
    private static boolean canBeMerged(final Range[] ranges) {
        int maxUpper = ranges[0].upper;
        for (int i = 1; i < ranges.length; ++i) {
            if (ranges[i].lower <= maxUpper + 1)
                return true;
            maxUpper = Math.max(maxUpper, ranges[i].upper);
        }

        return false;
    }

    // N.B. This function assumes that the lower ends of the input ranges "ranges" are monotonically increasing.  This
    // will be preserved for the return value
    public static Range[] merge(final Range[] ranges) {
        if (!Range.canBeMerged(ranges)) {
            return ranges;
        }

        final Range[] mergedRanges = new Range[ranges.length - 1];
        Range mergedRange;
        int sourceIndex = 0;
        int targetIndex = 0;
        do {
            mergedRange = ranges[sourceIndex];
            while (++sourceIndex < ranges.length && ranges[sourceIndex].lower <= mergedRange.upper + 1) {
                mergedRange.upper = Math.max(mergedRange.upper, ranges[sourceIndex].upper);
            }
            mergedRanges[targetIndex] = mergedRange;
            ++targetIndex;
        } while (sourceIndex < ranges.length);

        if (targetIndex == mergedRanges.length) {
            return mergedRanges;
        }
        return Arrays.copyOf(mergedRanges, targetIndex);
    }

    public Range(int lower, int upper) {
        this.lower = Math.min(lower, upper);
        this.upper = Math.max(lower, upper);
    }

    public final int getLower() {
        return lower;
    }

    public final int getUpper() {
        return upper;
    }

    public float getMatchingScore(final Range other) {
        final float numerator = Math.min(upper, other.upper) - Math.max(lower, other.lower) + 1;
        final float denominator = Math.max(upper, other.upper) - Math.min(lower, other.lower) + 1;
        return numerator / denominator;
    }

    public boolean intersects(Range other) {
        return other.upper >= lower && other.lower <= upper;
    }

    // N.B. This function assumes that the lower ends of the input ranges are monotonically increasing.
    public float getBestMatchingScore(Range[] targets) {
        float queryDistance = Float.NEGATIVE_INFINITY;
        for (final Range target : targets) {
            queryDistance = Math.max(getMatchingScore(target), queryDistance);
        }
        return queryDistance;
    }

    @Override
    public String toString() {
        return "[" + lower + " - " + upper + "]";
    }

    static public String toString(final Range[] ranges) {
        StringBuilder builder = new StringBuilder();
        builder.append('[');
        for (final Range range : ranges) {
            builder.append(range.toString());
        }
        builder.append(']');
        return builder.toString();
    }
}
