package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import org.junit.Test;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLEncoder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

public class SearchTests {
    private final static String URL_PATTERN = "http://localhost:8080/solr/biblio/select?q={!bibleRangeParser}%s&wt=xml&indent=true";

    public String search(String bibleRangeString) throws UnsupportedEncodingException {
        String url = String.format(URL_PATTERN, URLEncoder.encode(bibleRangeString, "UTF-8"));
        System.out.println(url);
        StringBuilder sb = new StringBuilder(50_000);

        try {
            URLConnection connection = new URL(url).openConnection();
            BufferedReader in = new BufferedReader(new InputStreamReader(connection.getInputStream()));
            String line;
            while ((line = in.readLine()) != null) {
                sb.append(line).append('\n');
            }
            in.close();
        } catch (IOException e) {
            // handle exception
        }

        return sb.toString();
    }
    @Test
    public void testSearchPsalm_22_17() throws UnsupportedEncodingException {
        String result = search("6800000_6899999");
        System.out.println(result);
        assertFalse(result.isEmpty());
    }

    @Test
    public void testSearch() throws UnsupportedEncodingException {
        String result = search("*");
        assertEquals("23394", result.replaceAll(".*numFound=\"([0-9]*)\".*", "$1"));
    }

    @Test
    public void testJesusAndHisMother() throws UnsupportedEncodingException {
        String result = search("3000320_3000321,3000331_3000335");
        System.out.println();
        System.out.println();
        assertEquals("1077", result.replaceAll(".*numFound=\"([0-9]*)\".*", "$1"));
        assertTrue(result.contains("<str name=\"title\">Jesus and his mother according to Mk 3.20.21, 31-35</str>"));
    }
}
