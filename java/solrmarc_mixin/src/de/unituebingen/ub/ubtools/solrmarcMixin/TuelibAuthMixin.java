package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.Set;
import java.util.logging.Logger;

import org.apache.commons.lang3.StringUtils;
import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.marc4j.marc.Subfield;
import org.marc4j.marc.VariableField;

public class TuelibAuthMixin extends TuelibMixin {

    protected final static Logger logger = Logger.getLogger(TuelibAuthMixin.class.getName());
    
    protected final static Pattern SORTABLE_STRING_REMOVE_PATTERN = Pattern.compile("[^\\p{Lu}\\p{Ll}\\p{Lt}\\p{Lo}\\p{N}]+");
    protected final static Pattern YEAR_RANGE_PATTERN = Pattern.compile("^(\\d+)-(\\d+)$");

    /**
     * normalize string due to specification for isni or orcid
     * @param input    input string to normalize
     * @param category category isni or orchid
     * @return normalized value depending on isni, orcid pattern
     */
    private String normalizeByCategory(String input, String category) {
        String stripped = stripDashesAndWhitespaces(input);
        if (stripped.length() != 16) {
            return input;
        } else {
            if (category.equalsIgnoreCase("isni")) {
                return stripped.substring(0, 4) + " " + stripped.substring(4, 8) + " " + stripped.substring(8, 12) + " " + stripped.substring(12, 16);
            } else if (category.equalsIgnoreCase("orcid")) {
                return stripped.substring(0, 4) + "-" + stripped.substring(4, 8) + "-" + stripped.substring(8, 12) + "-" + stripped.substring(12, 16);
            } else {
                return input;
            }
        }
    }

    private String stripDashesAndWhitespaces(String input) {
        return input.replaceAll("-", "").replaceAll("\\s", "");
    }

    /**
     * @param record          implicit call
     * @param tagNumber       e.g. 024, only use Datafields > 010
     * @param number2Category isni | orcid
     * @return
     */
    public String getNormalizedValueByTag2(final Record record, final String tagNumber, final String number2Category) {

        @SuppressWarnings("unchecked")
        List<DataField> mainFields = (List<DataField>) (List<?>) record.getVariableFields(tagNumber);
        mainFields.removeIf(m -> m.getSubfield('2') == null);
        mainFields.removeIf(m -> m.getSubfield('a') == null);
        mainFields.removeIf(m -> m.getSubfield('2').getData().equalsIgnoreCase(number2Category) == false);

        if (mainFields.size() == 0) {
            return null;
        } else if (mainFields.size() == 1) {
            return normalizeByCategory(mainFields.get(0).getSubfield('a').getData(), number2Category);
        } else {
            Set<String> differentNormalizedValues = new HashSet<String>();
            for (DataField mainField : mainFields) {
                final String numberA = mainField.getSubfield('a').getData();
                String normalizedValue = normalizeByCategory(numberA, number2Category);
                differentNormalizedValues.add(normalizedValue);
            }
            if (differentNormalizedValues.size() == 1) {
                return differentNormalizedValues.iterator().next();
            } else {
                logger.warning("record id " + record.getControlNumber() + " - multiple field with different content " + number2Category);
                return null;
            }

        }
    }   
    
    protected String normalizeSortableString(String string) {
        // Only keep letters & numbers. For unicode character classes, see:
        // https://en.wikipedia.org/wiki/Template:General_Category_(Unicode)
        if (string == null)
            return null;
        //c.f. https://stackoverflow.com/questions/1466959/string-replaceall-vs-matcher-replaceall-performance-differences (21/03/16)
        return SORTABLE_STRING_REMOVE_PATTERN.matcher(string).replaceAll("").trim();
    }
    
    public Collection<String> normalizeSortableString(Collection<String> extractedValues) {
        Collection<String> results = new ArrayList<String>();
        for (final String value : extractedValues) {
            final String newValue = normalizeSortableString(value);
            if (newValue != null && !newValue.isEmpty())
                results.add(StringUtils.stripAccents(newValue));
        }
        return results;
    }
    
    public String getYearRange(final Record record) {
        final List<VariableField> yearFields = record.getVariableFields("400");
        for (final VariableField yearField : yearFields) {
            final DataField field = (DataField) yearField;
            for (final Subfield subfield_d : field.getSubfields('d')) {
                final Matcher matcher = YEAR_RANGE_PATTERN.matcher(subfield_d.getData());
                if (matcher.matches())
                    return "[" + matcher.group(1) + " TO " + matcher.group(2) + "]";
            }
        }

        return null;
    }
    
    public String getAuthorityType(final Record record) {
        if (record.getVariableFields("100").size() > 0)
            return "person";
        if (record.getVariableFields("110").size() > 0)
            return "corporate";
        if (record.getVariableFields("111").size() > 0)
            return "meeting";
        return null;
    }
}
