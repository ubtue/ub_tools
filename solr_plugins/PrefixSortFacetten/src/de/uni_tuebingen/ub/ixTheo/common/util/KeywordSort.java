package de.uni_tuebingen.ub.ixTheo.common.util;

import java.util.ArrayList;
import java.util.Arrays;

public class KeywordSort {

    static int calculateCommonPrefixLength(final String s1, final String s2) {
        final int minLength = Math.min(s1.length(), s2.length());
        int commonPrefixLength = 0;

        for (/* Intentionally empty! */; commonPrefixLength < minLength; ++commonPrefixLength) {
            if (Character.toLowerCase(s1.charAt(commonPrefixLength)) != Character.toLowerCase(s2.charAt(commonPrefixLength)))
                return commonPrefixLength;
        }

        return commonPrefixLength;
    }

    /** \brief Splits a colon-separated String into individual components. */
    static void parseArg(final String arg, final ArrayList<String> components) {
        String[] parts = arg.split(":");
        for (String part : parts)
            components.add(part);
    }


   public static ArrayList<String> sortToReferenceChain(final ArrayList<String> refChainList, ArrayList<String> compChainList){

	String[] refchain = refChainList.toArray(new String[0]);
	String[] compchain = compChainList.toArray(new String[0]);

	for (int i = 0; i < Math.min(refchain.length, compchain.length); i++){

		String refstring = refchain[i];

		int max_prefix_score = 0;
		int max_prefix_index = i;

		for (int k = i; k < compchain.length; k++){
			
			String compstring = compchain[k];
			int prefix_score = calculateCommonPrefixLength(refstring, compstring);

			if ( prefix_score > max_prefix_score ){
				max_prefix_score = prefix_score;
				max_prefix_index = k;
			}
			
		}

		// Swap elements

		String tmp = compchain[i];
		compchain[i] = compchain[max_prefix_index];
		compchain[max_prefix_index] = tmp;

	}

	return new ArrayList<String>(Arrays.asList(compchain));
    }

}
