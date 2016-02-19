package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import org.junit.Test;

import static org.junit.Assert.*;


public class BibleRangeFilterTests {
    @Test
    public void testMatches() {
        assertFalse(new BibleRangeFilter(new BibleRange[]{}).matches(new BibleRange[]{}));
        assertTrue(new BibleRangeFilter(new BibleRange[]{new BibleRange("6802200_6802422")}).matches(new BibleRange[]{new BibleRange("6802217_6802422")}));
        assertTrue(new BibleRangeFilter(new BibleRange[]{new BibleRange("6800000_6899999")}).matches(new BibleRange[]{new BibleRange("6800000_6899999")}));
        assertFalse(new BibleRangeFilter(new BibleRange[]{new BibleRange("6802217_6802422")}).matches(new BibleRange[]{new BibleRange("6800000_6899999")}));
    }


}
