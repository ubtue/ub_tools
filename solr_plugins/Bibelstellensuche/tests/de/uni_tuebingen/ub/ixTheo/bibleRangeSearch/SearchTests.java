package de.uni_tuebingen.ub.ixTheo.bibleRangeSearch;

import org.junit.Test;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLEncoder;

import static org.junit.Assert.assertTrue;

/**
 * Created by quboo01 on 12.08.15.
 */
public class SearchTests {
    private final static String URL_PATTERN = "http://ptah.ub.uni-tuebingen.de:8080/solr/biblio/select?q={!bibleRangeParser}%s&wt=xml&indent=true";

    public String search(String bibleRangeString) throws UnsupportedEncodingException {
        String url = String.format(URL_PATTERN, URLEncoder.encode(bibleRangeString, "UTF-8"));

        StringBuilder sb = new StringBuilder();

        try {
            URLConnection connection = new URL(url).openConnection();
            BufferedReader in = new BufferedReader(new InputStreamReader(connection.getInputStream()));
            String line;
            while ((line = in.readLine()) != null) {
                sb.append(line);
            }
            in.close();
        } catch (IOException e) {
            // handle exception
        }

        return sb.toString();
    }

    @Test
    public void testSearch() throws UnsupportedEncodingException {
        String result = search("*");
        assertTrue(result.contains(" numFound=\"23221\""));
    }

    @Test
    public void testJesusAndHisMother() throws UnsupportedEncodingException {
        String result = search("2700320_2700321,2700331_2700335");
        assertTrue(result.contains("numFound=\"1669\""));
        assertTrue(result.contains("<str name=\"title\">Jesus and his mother according to Mk 3.20.21, 31-35 /</str>"));
    }
}
