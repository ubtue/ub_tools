package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;


import com.google.common.collect.Ordering;
import org.junit.Test;

import java.util.Arrays;
import java.util.HashMap;

import static org.junit.Assert.assertEquals;


public class ScoreTest {

    @Test(expected = AssertionError.class)
    public void testMultiBad_vs_OneGood() {
        final BibleRange[] queryRanges = BibleRange.getRanges(new String[]{"2500521_2500543"});

        BibleRange[] good = BibleRange.getRanges(new String[]{"2500521_2500543"});
        BibleRange[] bad = new BibleRange[50];
        for (int i = 0; i < bad.length; i++) {
            bad[i] = new BibleRange("2500100_2500826");
        }

        HashMap<BibleRange[], Integer> expectedRanking = new HashMap<>();
        expectedRanking.put(good, 0);
        expectedRanking.put(bad, 1);

        BibleRange[][] ranges = new BibleRange[][]{good, bad};

        sortBySoring(ranges, queryRanges);
        assertRanking(expectedRanking, ranges);

        printScoring(ranges, queryRanges);
    }

    @Test
    public void testMarkus5_21_to_43() {
        final BibleRange[] queryRanges = BibleRange.getRanges(new String[]{"25005021_25005043"});

        BibleRange[] r0 = BibleRange.getRanges(new String[]{"25005021_25005043"});
        BibleRange[] r1 = BibleRange.getRanges(new String[]{"25004000_25008099"});
        BibleRange[] r2 = BibleRange.getRanges(new String[]{"25001000_25008026"});
        BibleRange[] r3 = BibleRange.getRanges(new String[]{"25001000_25008099"});
        BibleRange[] r4 = BibleRange.getRanges(new String[]{"25001000_25013099"});
        BibleRange[] r5 = BibleRange.getRanges(new String[]{"01009020_01009022", "05008043_05008048", "25005025_25005034"});
        BibleRange[] r6 = BibleRange.getRanges(new String[]{"25005021_25005024", "25005035_25005043"});

        HashMap<BibleRange[], Integer> expectedRanking = new HashMap<>();
        expectedRanking.put(r0, 0);
        expectedRanking.put(r6, 1);
        expectedRanking.put(r5, 2);
        expectedRanking.put(r1, 3);
        expectedRanking.put(r2, 4);
        expectedRanking.put(r3, 5);
        expectedRanking.put(r4, 6);

        BibleRange[][] ranges = new BibleRange[][]{r0, r1, r2, r3, r4, r5, r6};
        sortBySoring(ranges, queryRanges);
        assertRanking(expectedRanking, ranges);
    }


    @Test
    public void testPsalm_22_2() {
        final BibleRange[] queryRanges = BibleRange.getRanges(new String[]{"68022002_68022002"});

        BibleRange[] r0 = BibleRange.getRanges(new String[]{"68001000_68050099"});
        BibleRange[] r1 = BibleRange.getRanges(new String[]{"68022000_68022099"});
        BibleRange[] r2 = BibleRange.getRanges(new String[]{"68001000_68041099"});
        BibleRange[] r3 = BibleRange.getRanges(new String[]{"68001000_68072099"});

        HashMap<BibleRange[], Integer> expectedRanking = new HashMap<>();
        expectedRanking.put(r0, 2);
        expectedRanking.put(r1, 0);
        expectedRanking.put(r2, 1);
        expectedRanking.put(r3, 3);

        BibleRange[][] ranges = new BibleRange[][]{r0, r1, r2, r3};
        printScoring(ranges, queryRanges);
        sortBySoring(ranges, queryRanges);
        assertRanking(expectedRanking, ranges);
    }

    /**
     * Prints the scoring for each list of bible ranges of fieldRanges.
     *
     * For test development. Do not remove!
     */
    private void printScoring(BibleRange[][] fieldRanges, BibleRange[] queryRanges) {
        for (int i = 0; i < fieldRanges.length; i++) {
            System.out.println(i + ": " + BibleRange.getMatchingScore(fieldRanges[i], queryRanges));
        }
    }

    private void sortBySoring(BibleRange[][] fieldRanges, final BibleRange[] queryRanges) {
        Arrays.sort(fieldRanges, new Ordering<BibleRange[]>() {
            @Override
            public int compare(final BibleRange[] t1, final BibleRange[] t2) {
                return -Float.compare(BibleRange.getMatchingScore(t1, queryRanges), BibleRange.getMatchingScore(t2, queryRanges));
            }
        });
    }

    private void assertRanking(HashMap<BibleRange[], Integer> expected, BibleRange[][] fieldRanges) {
        for (int i = 0; i < fieldRanges.length; i++) {
            assertEquals(expected.get(fieldRanges[i]).intValue(), i);
        }
    }
}
