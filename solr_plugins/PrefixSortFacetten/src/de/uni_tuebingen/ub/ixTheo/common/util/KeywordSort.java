package de.uni_tuebingen.ub.ixTheo.common.util;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;


public class KeywordSort {
    private static int calculateCommonPrefixLength(final String s1, final String s2) {
        final int minLength = Math.min(s1.length(), s2.length());
        for (int commonPrefixLength = 0; commonPrefixLength < minLength; ++commonPrefixLength) {
            if (Character.toLowerCase(s1.charAt(commonPrefixLength)) != Character
                    .toLowerCase(s2.charAt(commonPrefixLength)))
                return commonPrefixLength;
        }

        return minLength;
    }

    public static ArrayList<String> sortToReferenceChain(final ArrayList<String> refChainList,
            ArrayList<String> compChainList) {

        for (int i = 0; i < Math.min(refChainList.size(), compChainList.size()); i++) {
            String refstring = refChainList.get(i);
            int max_prefix_score = 0;
            int max_prefix_index = i;

            for (int k = i; k < compChainList.size(); k++) {
                String compstring = compChainList.get(k);
                int prefix_score = calculateCommonPrefixLength(refstring, compstring);

                if (prefix_score > max_prefix_score) {
                    max_prefix_score = prefix_score;
                    max_prefix_index = k;
                }
            }
            Collections.swap(compChainList, i, max_prefix_index);
        }
        return compChainList;
    }
}
