package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import java.util.Arrays;

/**
 * \class Range Represents a bible range.
 */
class Range {
    private int lower;
    private int upper;

    public static float getMatchingScore(final Range[] fieldRanges, final Range[] queryRanges) {
        float distance = 0;
        for (final Range fieldRange : fieldRanges) {
            distance += Math.max(0, fieldRange.getBestMatchingScore(queryRanges));
        }
        return distance;
    }

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

    public static boolean canBeMerged(final Range[] ranges) {
        for (int i = 1; i < ranges.length; ++i) {
            if (ranges[i - 1].upper >= ranges[i].lower) {
                return true;
            }
        }
        
        return false;
    }
    
    public static Range[] Intersect(final Range[] ranges) {
        if (!Range.canBeMerged(ranges)) {
            return ranges;
        }

        final Range[] mergedRanges = new Range[ranges.length];
        Range mergedRange;
        int sourceIndex = 0;
        int targetIndex = 0;
        do {
            mergedRange = ranges[sourceIndex];
            while (++sourceIndex < ranges.length && ranges[sourceIndex].intersects(mergedRange)) {
                mergedRange.upper = Math.max(mergedRange.upper, ranges[sourceIndex].upper);
            }
            mergedRanges[targetIndex] = mergedRange;
            ++targetIndex;
        } while (sourceIndex < ranges.length);
        
        if (targetIndex == mergedRanges.length - 1) {
            return mergedRanges;
        }
        return Arrays.copyOf(mergedRanges, targetIndex + 1);
    }

    public Range(int lower, int upper) {
        this.lower = lower;
        this.upper = upper;
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

    public String toString(final Range[] ranges) {
        StringBuilder builder = new StringBuilder();
        builder.append('[');
        for (final Range range : ranges) {
            builder.append(range.toString());
        }
        builder.append(']');
        return builder.toString();
    }
}
