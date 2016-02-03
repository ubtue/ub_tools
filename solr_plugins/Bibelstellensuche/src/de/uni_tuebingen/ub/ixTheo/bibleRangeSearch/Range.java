package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


/**
 * \class Range Represents a bible range.
 */
class Range {
    private final int lower;
    private final int upper;

    public static float getMatchingScore(final Range[] fieldRanges, final Range[] queryRanges) {
        float distance = 0;
        for (final Range fieldRange : fieldRanges) {
            distance += fieldRange.getBestMatchingScore(queryRanges);
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
        final float numerator = Math.min(upper, other.upper) - Math.max(lower, other.lower);
        final float denominator = Math.max(upper, other.upper) - Math.min(lower, other.lower);
        return numerator / denominator;
    }

    public boolean intersects(Range other) {
        return other.upper >= lower && other.lower <= upper;
    }

    public float getBestMatchingScore(Range[] targets) {
        float queryDistance = 0;
        for (final Range target : targets) {
                queryDistance = Math.max(getMatchingScore(target), queryDistance);
        }
        return queryDistance;
    }

    @Override
    public String toString() {
        return "[" + lower + " - " + upper + "]";
    }
}
