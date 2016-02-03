package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import org.junit.Test;

import static org.junit.Assert.*;


public class BibleRangeTests {
    private Range r1, r2;
    private Range[] rs1, rs2;

    @Test
    public void testGetMatchingScore() {
        r1 = new Range(0, 2);
        r2 = new Range(2, 4);
        assertEquals(0f, r1.getMatchingScore(r2), 0.001);

        r1 = new Range(0, 2);
        r2 = new Range(1, 2);
        assertEquals(0.5f, r1.getMatchingScore(r2), 0.001);

        r1 = new Range(0, 3);
        r2 = new Range(1, 2);
        assertEquals(0.33333f, r1.getMatchingScore(r2), 0.001);

        r1 = new Range(0, 2);
        r2 = new Range(5, 7);
        assertEquals(-0.4286f, r1.getMatchingScore(r2), 0.001);
    }

    @Test
    public void testStaticGetMatchingScore() {
        rs1 = new Range[]{
                new Range(2700320, 2700321),
                new Range(2700331, 2700335),
        };
        rs2 = new Range[]{
                new Range(2700320, 2700321),
                new Range(2700331, 2700335),
        };
        assertEquals(2f, Range.getMatchingScore(rs1, rs2), 0.001);

        rs1 = new Range[]{
                new Range(2700320, 2700321),
                new Range(2700331, 2700335),
        };
        rs2 = new Range[]{
                new Range(100000, 199999),
                new Range(2700000, 2799999),
        };
        assertEquals(5E-5f, Range.getMatchingScore(rs1, rs2), 0.001);

        rs1 = new Range[]{
                new Range(2700320, 2700321),
                new Range(2700331, 2700335),
        };
        rs2 = new Range[]{
                new Range(2700424, 2700424)
        };
        assertEquals(0f, Range.getMatchingScore(rs1, rs2), 0.001);
    }

    @Test
    public void testGetBestMatchingScore() {
        Range r1;
        r1 = new Range(0, 2);
        float distance = r1.getBestMatchingScore(new Range[]{
                new Range(10, 11), new Range(-100, 100), new Range(0, 1), new Range(-2, 4)
        });
        assertEquals(0.5f, distance, 0.001);
    }

    @Test
    public void testIntersects() {
        r1 = new Range(0, 2);
        r2 = new Range(1, 2);
        assertTrue(r1.intersects(r2));

        r1 = new Range(0, 3);
        r2 = new Range(1, 2);
        assertTrue(r1.intersects(r2));

        r1 = new Range(0, 2);
        r2 = new Range(2, 4);
        assertTrue(r1.intersects(r2));

        r1 = new Range(0, 2);
        r2 = new Range(5, 7);
        assertFalse(r1.intersects(r2));
    }

    @Test
    public void testConstructor() {
        r1 = new BibleRange("0000001:0000002");
        assertEquals(1, r1.getLower());
        assertEquals(2, r1.getUpper());

        r1 = new BibleRange("0000010:0000020");
        assertEquals(10, r1.getLower());
        assertEquals(20, r1.getUpper());

        r1 = new BibleRange("2700320:2700321");
        assertEquals(2700320, r1.getLower());
        assertEquals(2700321, r1.getUpper());

        r1 = new BibleRange("*:*");
        assertEquals(0, r1.getLower());
        assertEquals(9999999, r1.getUpper());
    }

    @Test
    public void testHasIntersections() {
        rs1 = new Range[]{new Range(1, 2), new Range(5, 6)};
        rs2 = new Range[]{new Range(0, 2), new Range(3, 4), new Range(6, 10)};
        assertTrue(Range.hasIntersections(rs1, rs2));

        rs1 = new Range[]{new Range(1, 2), new Range(5, 6)};
        rs2 = new Range[]{new Range(0, 10)};
        assertTrue(Range.hasIntersections(rs1, rs2));

        rs1 = new Range[]{new Range(1, 2), new Range(5, 6)};
        rs2 = new Range[]{new Range(0, 3)};
        assertTrue(Range.hasIntersections(rs1, rs2));

        rs1 = new Range[]{new Range(1, 2), new Range(5, 6)};
        rs2 = new Range[]{new Range(10, 13)};
        assertFalse(Range.hasIntersections(rs1, rs2));
    }

    @Test
    public void testGetRangesFromField() {
        String field = "0000001_0000002 0000010_0000020";
        Range[] ranges = BibleRange.getRanges(field, " ");

        r1 = ranges[0];
        assertEquals(1, r1.getLower());
        assertEquals(2, r1.getUpper());

        r1 = ranges[1];
        assertEquals(10, r1.getLower());
        assertEquals(20, r1.getUpper());
    }

    @Test
    public void testGetRangeFromStrings() {
        String[] field = {"0000001_0000002", "0000010_0000020"};
        Range[] ranges = BibleRange.getRanges(field);

        r1 = ranges[0];
        assertEquals(1, r1.getLower());
        assertEquals(2, r1.getUpper());

        r1 = ranges[1];
        assertEquals(10, r1.getLower());
        assertEquals(20, r1.getUpper());
    }
}
