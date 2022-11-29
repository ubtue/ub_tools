package de.uni_tuebingen.ub.ixTheo.rangeSearch;

import java.time.DateTimeException;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * \class Range represents a range in a hierarchy of ranges.  An example would be parts of the Bible.
 */
public class Range {
    private long lower;
    private long upper;

    private static float getRangesScore(final Range[] ranges, final Range[] queryRanges) {
        float best_individual_distance = Float.NEGATIVE_INFINITY;
        int best_distance_count = 0;
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
        long maxUpper = ranges[0].upper;
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

    public Range(long lower, long upper) {
        this.lower = Math.min(lower, upper);
        this.upper = Math.max(lower, upper);
    }

    public final long getLower() {
        return lower;
    }

    public final long getUpper() {
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

    static Range[] StringToRanges(final String range_list) {
        final String[] parts = range_list.split(",");
        final ArrayList<Range> list = new ArrayList<Range>(parts.length);

        for (final String part : parts) {
            final String[] range = part.split(":");
            if (range.length != 2) {
                System.err.println(range + " is not a valid range! (1)");
                System.exit(-1);
            }

            try {
                final long lower = Long.parseLong(range[0]);
                final long upper = Long.parseLong(range[1]);
                final Range new_range = new Range(lower, upper);
                list.add(new_range);
            } catch (NumberFormatException e) {
                System.err.println(range + " is not a valid range! (2)");
                System.exit(-1);
            }
        }

        final Range[] ranges = new Range[list.size()];
        return list.toArray(ranges);
    }

    public static void main(String[] args) {
        if (args.length != 2) {
            System.err.println("Usage: comma_separated_ranges comma_separated_query_ranges "
                               + "(lower and upper are separated by a colon)");
            System.exit(-1);
        }

        final Range[] ranges = StringToRanges(args[0]);
        final Range[] queryRanges = StringToRanges(args[1]);
        System.out.println("Score: " + Float.toString(getRangesScore(ranges, queryRanges)));
    }
}
