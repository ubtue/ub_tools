package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.util.*;
import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.marc4j.marc.Subfield;
import org.marc4j.marc.VariableField;
import org.solrmarc.index.SolrIndexerMixin;
import de.unituebingen.ub.ubtools.solrmarcMixin.*;

public class IxTheoKeywordChains extends SolrIndexerMixin {

    private final static String KEYWORD_DELIMITER = "/";
    private final static String SUBFIELD_CODES = "abctnp";
    private final static TuelibMixin tuelibMixin = new TuelibMixin();

    public Set<String> getKeyWordChain(final Record record, final String fieldSpec, final String lang) {
        final List<VariableField> variableFields = record.getVariableFields(fieldSpec);
        final Map<Character, List<String>> keyWordChains = new HashMap<>();

        for (final VariableField variableField : variableFields) {
            final DataField dataField = (DataField) variableField;
            processField(dataField, keyWordChains, lang);
        }
        return concatenateKeyWordsToChains(keyWordChains);
    }

    /**
     * Create set version of the terms contained in the keyword chains
     */

    public Set<String> getKeyWordChainBag(final Record record, final String fieldSpec, final String lang) {
        final List<VariableField> variableFields = record.getVariableFields(fieldSpec);
        final Map<Character, List<String>> keyWordChains = new HashMap<>();
        final Set<String> keyWordChainBag = new HashSet<>();

        for (final VariableField variableField : variableFields) {
            final DataField dataField = (DataField) variableField;
            processField(dataField, keyWordChains, lang);
        }

        for (List<String> keyWordChain : keyWordChains.values()) {
            keyWordChainBag.addAll(keyWordChain);
        }

        return keyWordChainBag;
    }

    public Set<String> getKeyWordChainSorted(final Record record, final String fieldSpec, final String lang) {
        final List<VariableField> variableFields = record.getVariableFields(fieldSpec);
        final Map<Character, List<String>> keyWordChains = new HashMap<>();

        for (final VariableField variableField : variableFields) {
            final DataField dataField = (DataField) variableField;
            processField(dataField, keyWordChains, lang);

            // Sort keyword chain
            final char chainID = dataField.getIndicator1();
            final List<String> keyWordChain = getKeyWordChain(keyWordChains, chainID);
            Collections.sort(keyWordChain);
        }
        return concatenateKeyWordsToChains(keyWordChains);
    }

    /**
     * Extracts the keyword from data field and inserts it into the right
     * keyword chain.
     */
    private void processField(final DataField dataField, final Map<Character, List<String>> keyWordChains, String lang) {
        final char chainID = dataField.getIndicator1();
        final List<String> keyWordChain = getKeyWordChain(keyWordChains, chainID);

        boolean gnd_seen = false;
        StringBuilder keyword = new StringBuilder();
        for (final Subfield subfield : dataField.getSubfields()) {
            if (gnd_seen) {
                if (SUBFIELD_CODES.indexOf(subfield.getCode()) != -1) {
                    if (keyword.length() > 0) {
                        if (subfield.getCode() == 'n') {
                            keyword.append(" ");
                        }
                        else if (subfield.getCode() == 'p') {
                            keyword.append(". ");
                        }
                        else {
                            keyword.append(", ");
                        }
                    }
                    keyword.append(tuelibMixin.translateTopic(subfield.getData(), lang));
                } else if (subfield.getCode() == '9' && keyword.length() > 0 && subfield.getData().startsWith("g:")) {
                    // For Ixtheo-translations the specification in the g:-Subfield is appended in angle
                    // brackets, so this is a special case where we have to begin from scratch
                    final String specification = subfield.getData().substring(2);
                    final Subfield germanASubfield = dataField.getSubfield('a');
                    if (germanASubfield != null) {
                        final String translationCandidate = germanASubfield.getData() + " <" + specification + ">";
                        final String translation = tuelibMixin.translateTopic(translationCandidate, lang);
                        if (translation != translationCandidate) {
                            keyword.setLength(0);
                            keyword.append(translation.replaceAll("<", "(").replaceAll(">", ")"));
                        }
                    }
                    else {
                        keyword.append(" (");
                        keyword.append(tuelibMixin.translateTopic(specification, lang));
                        keyword.append(')');
                    }
                }
            } else if (subfield.getCode() == '2' && subfield.getData().equals("gnd"))
                gnd_seen = true;
        }

        if (keyword.length() > 0) {
            String keywordString = keyword.toString();
            keyWordChain.add(keywordString);
        }
    }

    /**
     * Finds the right keyword chain for a given chain id.
     *
     * @return A map containing the keywords of the chain (id -> keyword), or an
     *         empty map.
     */
    private List<String> getKeyWordChain(final Map<Character, List<String>> keyWordChains, final char chainID) {
        List<String> keyWordChain = keyWordChains.get(chainID);
        if (keyWordChain == null) {
            keyWordChain = new ArrayList<>();
            keyWordChains.put(chainID, keyWordChain);
        }

        return keyWordChain;
    }

    private Set<String> concatenateKeyWordsToChains(final Map<Character, List<String>> keyWordChains) {
        final List<Character> chainIDs = new ArrayList<>(keyWordChains.keySet());
        Collections.sort(chainIDs);

        final Set<String> chainSet = new LinkedHashSet<>();
        for (final Character chainID : chainIDs) {
            chainSet.add(keyChainToString(keyWordChains.get(chainID)));
        }
        return chainSet;
    }

    private String keyChainToString(final List<String> keyWordChain) {
        final StringBuilder buffer = new StringBuilder();
        for (final String keyWord : keyWordChain) {
            buffer.append(KEYWORD_DELIMITER);
            buffer.append(keyWord);
        }

        if (buffer.length() == 0) {
            return "";
        }
        // Discard leading keyword delimiter.
        return buffer.toString().substring(1);
    }
}
