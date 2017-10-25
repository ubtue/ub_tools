package de.unituebingen.ub.ubtools.solrmarcMixin;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.logging.Logger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.marc4j.marc.ControlField;
import org.marc4j.marc.DataField;
import org.marc4j.marc.Record;
import org.marc4j.marc.Subfield;
import org.marc4j.marc.VariableField;
import org.solrmarc.index.SolrIndexer;
import org.solrmarc.index.SolrIndexerMixin;
import org.solrmarc.tools.DataUtil;
import org.solrmarc.tools.Utils;

public class TuelibMixin extends SolrIndexerMixin {
    public final static String UNASSIGNED_STRING = "[Unassigned]";
    public final static Set<String> UNASSIGNED_SET = Collections.singleton(UNASSIGNED_STRING);

    private final static Logger logger = Logger.getLogger(TuelibMixin.class.getName());
    private final static String UNKNOWN_MATERIAL_TYPE = "Unbekanntes Material";
    private final static String VALID_FOUR_DIGIT_YEAR = "\\d{4}";

    private final static Pattern PAGE_RANGE_PATTERN1 = Pattern.compile("\\s*(\\d+)\\s*-\\s*(\\d+)$");
    private final static Pattern PAGE_RANGE_PATTERN2 = Pattern.compile("\\s*\\[(\\d+)\\]\\s*-\\s*(\\d+)$");
    private final static Pattern PAGE_RANGE_PATTERN3 = Pattern.compile("\\s*(\\d+)\\s*ff");
    private final static Pattern PPN_EXTRACTION_PATTERN = Pattern.compile("^\\([^)]+\\)(.+)$");
    private final static Pattern START_PAGE_MATCH_PATTERN = Pattern.compile("\\[?(\\d+)\\]?(-\\d+)?");
    private final static Pattern VALID_FOUR_DIGIT_PATTERN = Pattern.compile(VALID_FOUR_DIGIT_YEAR);
    private final static Pattern VOLUME_PATTERN = Pattern.compile("^\\s*(\\d+)$");
    private final static Pattern YEAR_PATTERN = Pattern.compile("(\\d\\d\\d\\d)");

    // TODO: This should be in a translation mapping file
    private final static HashMap<String, String> isil_to_department_map = new HashMap<String, String>() {
        {
            this.put("Unknown", "Unknown");
            this.put("DE-21", "Universit\u00E4tsbibliothek T\u00FCbingen");
            this.put("DE-21-1", "Universit\u00E4t T\u00FCbingen, Klinik f\u00FCr Psychatrie und Psychologie");
            this.put("DE-21-3", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Toxikologie und Pharmakologie");
            this.put("DE-21-4", "Universit\u00E4t T\u00FCbingen, Universit\u00E4ts-Augenklinik");
            this.put("DE-21-10", "Universit\u00E4tsbibliothek T\u00FCbingen, Bereichsbibliothek Geowissenschaften");
            this.put("DE-21-11", "Universit\u00E4tsbibliothek T\u00FCbingen, Bereichsbibliothek Schloss Nord");
            this.put("DE-21-14",
                    "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Ur- und Fr\u00FChgeschichte und Arch\u00E4ologie des Mittelalters, Abteilung j\u00FCngere Urgeschichte und Fr\u00FChgeschichte + Abteilung f\u00FCr Arch\u00E4ologie des Mittelalters");
            this.put("DE-21-17", "Universit\u00E4t T\u00FCbingen, Geographisches Institut");
            this.put("DE-21-18", "Universit\u00E4t T\u00FCbingen, Universit\u00E4ts-Hautklinik");
            this.put("DE-21-19", "Universit\u00E4t T\u00FCbingen, Wirtschaftswissenschaftliches Seminar");
            this.put("DE-21-20", "Universit\u00E4t T\u00FCbingen, Frauenklinik");
            this.put("DE-21-21", "Universit\u00E4t T\u00FCbingen, Universit\u00E4ts-Hals-Nasen-Ohrenklinik, Bibliothek");
            this.put("DE-21-22", "Universit\u00E4t T\u00FCbingen, Kunsthistorisches Institut");
            this.put("DE-21-23", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Pathologie");
            this.put("DE-21-24", "Universit\u00E4t T\u00FCbingen, Juristisches Seminar");
            this.put("DE-21-25", "Universit\u00E4t T\u00FCbingen, Musikwissenschaftliches Institut");
            this.put("DE-21-26", "Universit\u00E4t T\u00FCbingen, Anatomisches Institut");
            this.put("DE-21-27", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Anthropologie und Humangenetik");
            this.put("DE-21-28", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Astronomie und Astrophysik, Abteilung Astronomie");
            this.put("DE-21-31", "Universit\u00E4t T\u00FCbingen, Evangelisch-theologische Fakult\u00E4t");
            this.put("DE-21-32a", "Universit\u00E4t T\u00FCbingen, Historisches Seminar, Abteilung f\u00FCr Alte Geschichte");
            this.put("DE-21-32b", "Universit\u00E4t T\u00FCbingen, Historisches Seminar, Abteilung f\u00FCr Mittelalterliche Geschichte");
            this.put("DE-21-32c", "Universit\u00E4t T\u00FCbingen, Historisches Seminar, Abteilung f\u00FCr Neuere Geschichte");
            this.put("DE-21-34", "Universit\u00E4t T\u00FCbingen, Asien-Orient-Institut, Abteilung f\u00FCr Indologie und Vergleichende Religionswissenschaft");
            this.put("DE-21-35", "Universit\u00E4t T\u00FCbingen, Katholisch-theologische Fakult\u00E4t");
            this.put("DE-21-39", "Universit\u00E4t T\u00FCbingen, Fachbibliothek Mathematik und Physik / Bereich Mathematik");
            this.put("DE-21-37", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Sportwissenschaft");
            this.put("DE-21-42", "Universit\u00E4t T\u00FCbingen, Asien-Orient-Institut, Abteilung f\u00FCr Orient- uns Islamwissenschaft");
            this.put("DE-21-43", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Erziehungswissenschaft");
            this.put("DE-21-45", "Universit\u00E4t T\u00FCbingen, Philologisches Seminar");
            this.put("DE-21-46", "Universit\u00E4t T\u00FCbingen, Philosophisches Seminar");
            this.put("DE-21-50", "Universit\u00E4t T\u00FCbingen, Physiologisches Institut");
            this.put("DE-21-51", "Universit\u00E4t T\u00FCbingen, Psychologisches Institut");
            this.put("DE-21-52", "Universit\u00E4t T\u00FCbingen, Ludwig-Uhland-Institut f\u00FCr Empirische Kulturwissenschaft");
            this.put("DE-21-53", "Universit\u00E4t T\u00FCbingen, Asien-Orient-Institut, Abteilung f\u00FCr Ethnologie");
            this.put("DE-21-54", "Universit\u00E4t T\u00FCbingen, Universit\u00E4tsklinik f\u00FCr Zahn-, Mund- und Kieferheilkunde");
            this.put("DE-21-58", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Politikwissenschaft");
            this.put("DE-21-62", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Osteurop\u00E4ische Geschichte und Landeskunde");
            this.put("DE-21-63", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Tropenmedizin");
            this.put("DE-21-64", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Geschichtliche Landeskunde und Historische Hilfswissenschaften");
            this.put("DE-21-65", "Universit\u00E4t T\u00FCbingen, Universit\u00E4ts-Apotheke");
            this.put("DE-21-74", "Universit\u00E4t T\u00FCbingen, Zentrum f\u00FCr Informations-Technologie");
            this.put("DE-21-78", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Medizinische Biometrie");
            this.put("DE-21-81", "Universit\u00E4t T\u00FCbingen, Inst. f. Astronomie und Astrophysik/Abt. Geschichte der Naturwiss.");
            this.put("DE-21-85", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Soziologie");
            this.put("DE-21-86", "Universit\u00E4t T\u00FCbingen, Zentrum f\u00FCr Datenverarbeitung");
            this.put("DE-21-89", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Arbeits- und Sozialmedizin");
            this.put("DE-21-92", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Gerichtliche Medizin");
            this.put("DE-21-93", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Ethik und Geschichte der Medizin");
            this.put("DE-21-95", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Hirnforschung");
            this.put("DE-21-98", "Universit\u00E4t T\u00FCbingen, Fachbibliothek Mathematik und Physik / Bereich Physik");
            this.put("DE-21-99",
                    "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Ur- und Fr\u00FChgeschichte und Arch\u00E4ologie des Mittelalters, Abteilung f\u00FCr \u00E4ltere Urgeschichteund Quart\u00E4r\u00F6kologie");
            this.put("DE-21-106", "Universit\u00E4t T\u00FCbingen, Seminar f\u00FCr Zeitgeschichte");
            this.put("DE-21-108", "Universit\u00E4t T\u00FCbingen, Fakult\u00E4tsbibliothek Neuphilologie");
            this.put("DE-21-109", "Universit\u00E4t T\u00FCbingen, Asien-Orient-Institut, Abteilung f\u00FCr Sinologie und Koreanistik");
            this.put("DE-21-110", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Kriminologie");
            this.put("DE-21-112", "Universit\u00E4t T\u00FCbingen, Fakult\u00E4t f\u00FCr Biologie, Bibliothek");
            this.put("DE-21-116", "Universit\u00E4t T\u00FCbingen, Zentrum f\u00FCr Molekularbiologie der Pflanzen, Forschungsgruppe Pflanzenbiochemie");
            this.put("DE-21-117", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Medizinische Informationsverarbeitung");
            this.put("DE-21-118", "Universit\u00E4t T\u00FCbingen, Universit\u00E4ts-Archiv");
            this.put("DE-21-119", "Universit\u00E4t T\u00FCbingen, Wilhelm-Schickard-Institut f\u00FCr Informatik");
            this.put("DE-21-120", "Universit\u00E4t T\u00FCbingen, Asien-Orient-Institut, Abteilung f\u00FCr Japanologie");
            this.put("DE-21-121", "Universit\u00E4t T\u00FCbingen, Internationales Zentrum f\u00FCr Ethik in den Wissenschaften");
            this.put("DE-21-123", "Universit\u00E4t T\u00FCbingen, Medizinbibliothek");
            this.put("DE-21-124", "Universit\u00E4t T\u00FCbingen, Institut f. Medizinische Virologie und Epidemiologie d. Viruskrankheiten");
            this.put("DE-21-126", "Universit\u00E4t T\u00FCbingen, Institut f\u00FCr Medizinische Mikrobiologie und Hygiene");
            this.put("DE-21-203", "Universit\u00E4t T\u00FCbingen, Sammlung Werner Schweikert - Archiv der Weltliteratur");
            this.put("DE-21-205", "Universit\u00E4t T\u00FCbingen, Zentrum f\u00FCr Islamische Theologie");
            this.put("DE-Frei85", "Freiburg MPI Ausl\u00E4nd.Recht, Max-Planck-Institut f\u00FCr ausl\u00E4ndisches und internationales Strafrecht");
        }
    };

    // Map used by getPhysicalType().
    private static final Map<String, String> phys_code_to_full_name_map;

    static {
        Map<String, String> tempMap = new HashMap<>();
        tempMap.put("arbtrans", "Transparency");
        tempMap.put("blindendr", "Braille");
        tempMap.put("bray", "Blu-ray Disc");
        tempMap.put("cdda", "CD");
        tempMap.put("ckop", "Microfiche");
        tempMap.put("cofz", "Online Resource");
        tempMap.put("crom", "CD-ROM");
        tempMap.put("dias", "Slides");
        tempMap.put("disk", "Diskette");
        tempMap.put("druck", "Printed Material");
        tempMap.put("dvda", "Audio DVD");
        tempMap.put("dvdr", "DVD-ROM");
        tempMap.put("dvdv", "Video DVD");
        tempMap.put("gegenst", "Physical Object");
        tempMap.put("handschr", "Longhand Text");
        tempMap.put("kunstbl", "Artistic Works on Paper");
        tempMap.put("lkop", "Mircofilm");
        tempMap.put("medi", "Multiple Media Types");
        tempMap.put("scha", "Record");
        tempMap.put("skop", "Microform");
        tempMap.put("sobildtt", "Audiovisual Carriers");
        tempMap.put("soerd", "Carriers of Other Electronic Data");
        tempMap.put("sott", "Carriers of Other Audiodata");
        tempMap.put("tonbd", "Audiotape");
        tempMap.put("tonks", "Audiocasette");
        tempMap.put("vika", "Videocasette");
        phys_code_to_full_name_map = Collections.unmodifiableMap(tempMap);
    }

    private Set<String> isils_cache = null;
    private Set<String> reviews_cache = null;
    private Set<String> reviewedRecords_cache = null;

    public void perRecordInit(Record record) {
        reviews_cache = reviewedRecords_cache = isils_cache = null;
    }

    private String getTitleFromField(final DataField titleField) {
        if (titleField == null)
            return null;

        final String titleA = (titleField.getSubfield('a') == null) ? null : titleField.getSubfield('a').getData();
        final String titleB = (titleField.getSubfield('b') == null) ? null : titleField.getSubfield('b').getData();
        if (titleA == null && titleB == null)
            return null;

        final StringBuilder completeTitle = new StringBuilder();
        if (titleA == null)
            completeTitle.append(DataUtil.cleanData(titleB));
        else if (titleB == null || !titleA.endsWith(":"))
            completeTitle.append(DataUtil.cleanData(titleA));
        else { // Neither titleA nor titleB are null.
            completeTitle.append(DataUtil.cleanData(titleA));
            if (!titleB.startsWith(" = "))
                completeTitle.append(" : ");
            completeTitle.append(DataUtil.cleanData(titleB));
        }

        final String titleN = (titleField.getSubfield('n') == null) ? null : titleField.getSubfield('n').getData();
        if (titleN != null) {
            completeTitle.append(' ');
            completeTitle.append(DataUtil.cleanData(titleN));
        }
        return completeTitle.toString();
    }

    /**
     * Determine Record Title
     *
     * @param record
     *            the record
     * @return String nicely formatted title
     */
    public String getMainTitle(final Record record) {
        final DataField mainTitleField = (DataField) record.getVariableField("245");
        return getTitleFromField(mainTitleField);
    }

    public Set<String> getOtherTitles(final Record record) {
        final List<VariableField> otherTitleFields = record.getVariableFields("246");

        final Set<String> otherTitles = new TreeSet<>();
        for (final VariableField otherTitleField : otherTitleFields) {
            final DataField dataField = (DataField) otherTitleField;
            if (dataField.getIndicator1() == '3' && dataField.getIndicator2() == '0')
                continue;
            final String otherTitle = getTitleFromField((DataField) otherTitleField);
            if (otherTitle != null)
                otherTitles.add(otherTitle);
        }

        return otherTitles;
    }

    /**
     * Determine Record Title Subfield
     *
     * @param record
     *            the record
     * @param subfield_code
     * @return String nicely formatted title subfield
     */
    public String getTitleSubfield(final Record record, final String subfield_code) {
        final DataField title = (DataField) record.getVariableField("245");
        if (title == null)
            return null;

        final Subfield subfield = title.getSubfield(subfield_code.charAt(0));
        if (subfield == null)
            return null;

        final String subfield_data = subfield.getData();
        if (subfield_data == null)
            return null;

        return DataUtil.cleanData(subfield_data);
    }

    static private Set<String> getAllSubfieldsBut(final Record record, final String fieldSpecList,
                                                  char excludeSubfield)
    {
        final Set<String> extractedValues = new TreeSet<>();
        final String[] fieldSpecs = fieldSpecList.split(":");
        List<Subfield> subfieldsToSearch = new ArrayList<>();
        for (final String fieldSpec : fieldSpecs) {
            final List<VariableField> fieldSpecFields = record.getVariableFields(fieldSpec.substring(0,3));
            for (final VariableField variableField : fieldSpecFields) {
                 final DataField field = (DataField) variableField;
                 if (field == null)
                     continue;
                 // Differentiate between field and subfield specifications:
                 if (fieldSpec.length() == 3 + 1)
                     subfieldsToSearch = field.getSubfields(fieldSpec.charAt(3));
                 else if (fieldSpec.length() == 3)
                     subfieldsToSearch = field.getSubfields();
                 else {
                     System.err.println("in TuelibMixin.getAllSubfieldsBut: invalid field specification: " + fieldSpec);
                     System.exit(1);
                 }
                 for (final Subfield subfield : subfieldsToSearch)
                     if (subfield.getCode() != excludeSubfield)
                         extractedValues.add(subfield.getData());
            }
        }
        return extractedValues;
    }

    /**
     * get the local subjects from LOK-tagged fields and get subjects from 936k
     * and 689a subfields
     * <p/>
     * LOK = Field |0 689 = Subfield |a Imperialismus = Subfield with local
     * subject
     *
     * @param record
     *            the record
     * @return Set of local subjects
     */
    public Set<String> getAllTopics(final Record record) {
        final Set<String> topics = getAllSubfieldsBut(record, "600:610:611:630:650:653:656:689a:936a", '0');
        for (final VariableField variableField : record.getVariableFields("LOK")) {
            final DataField lokfield = (DataField) variableField;
            final Subfield subfield0 = lokfield.getSubfield('0');
            if (subfield0 == null || !subfield0.getData().equals("689  ")) {
                continue;
            }
            for (final Subfield subfieldA : lokfield.getSubfields('a')) {
                if (subfieldA != null && subfieldA.getData() != null && subfieldA.getData().length() > 2) {
                    topics.add(subfieldA.getData());
                }
            }
        }
        return topics;
    }

    /**
     * Hole das Sachschlagwort aus 689|a (wenn 689|d != z oder f)
     *
     * @param record
     *            the record
     * @return Set "topic_facet"
     */
    public Set<String> getFacetTopics(final Record record) {
        final Set<String> result = getAllSubfieldsBut(record, "600x:610x:611x:630x:648x:650a:650x:651x:655x", '0');
        String topic_string;
        // Check 689 subfield a and d
        final List<VariableField> fields = record.getVariableFields("689");
        if (fields != null) {
            DataField dataField;
            for (final VariableField variableField : fields) {
                dataField = (DataField) variableField;
                final Subfield subfieldD = dataField.getSubfield('d');
                if (subfieldD == null) {
                    continue;
                }
                topic_string = subfieldD.getData().toLowerCase();
                if (topic_string.equals("f") || topic_string.equals("z")) {
                    continue;
                }
                final Subfield subfieldA = dataField.getSubfield('a');
                if (subfieldA != null) {
                    result.add(subfieldA.getData());
                }
            }
        }
        return result;
    }

    /**
     * Finds the first subfield which is nonempty.
     *
     * @param dataField
     *            the data field
     * @param subfieldIDs
     *            the subfield identifiers to search for
     * @return a nonempty subfield or null
     */
    private Subfield getFirstNonEmptySubfield(final DataField dataField, final char... subfieldIDs) {
        for (final char subfieldID : subfieldIDs) {
            for (final Subfield subfield : dataField.getSubfields(subfieldID)) {
                if (subfield != null && subfield.getData() != null && !subfield.getData().isEmpty()) {
                    return subfield;
                }
            }
        }
        return null;
    }

    /**
     * Returns either a Set<String> of parent (URL + colon + material type).
     * URLs are taken from 856$u and material types from 856$3, 856$z or 856$x.
     * For missing type subfields the text "Unbekanntes Material" will be used.
     * Furthermore 024$2 will be checked for "doi". If we find this we generate
     * a URL with a DOI resolver from the DOI in 024$a and set the
     * "material type" to "DOI Link".
     *
     * @param record
     *            the record
     * @return A, possibly empty, Set<String> containing the URL/material-type
     *         pairs.
     */
    public Set<String> getUrlsAndMaterialTypes(final Record record) {
        final Set<String> nonUnknownMaterialTypeURLs = new HashSet<String>();
        final Map<String, Set<String>> materialTypeToURLsMap = new TreeMap<String, Set<String>>();
        final Set<String> urls_and_material_types = new LinkedHashSet<>();
        for (final VariableField variableField : record.getVariableFields("856")) {
            final DataField field = (DataField) variableField;
            final Subfield materialTypeSubfield = getFirstNonEmptySubfield(field, '3', 'z', 'y', 'x');
            final String materialType = (materialTypeSubfield == null) ? UNKNOWN_MATERIAL_TYPE : materialTypeSubfield.getData();

            // Extract all links from u-subfields and resolve URNs:
            for (final Subfield subfield_u : field.getSubfields('u')) {
                Set<String> URLs = materialTypeToURLsMap.get(materialType);
                if (URLs == null) {
                    URLs = new HashSet<String>();
                    materialTypeToURLsMap.put(materialType, URLs);
                }

                final String rawLink = subfield_u.getData();
                final String link;
                if (rawLink.startsWith("urn:"))
                    link = "https://nbn-resolving.org/" + rawLink;
                else if (rawLink.startsWith("http://nbn-resolving.de"))
                    // Replace HTTP w/ HTTPS.
                    link = "https://nbn-resolving.org/" + rawLink.substring(23);


                else if (rawLink.startsWith("http://nbn-resolving.org"))
                    // Replace HTTP w/ HTTPS.
                    link = "https://nbn-resolving.org/" + rawLink.substring(24);
                else
                    link = rawLink;
                URLs.add(link);
                if (!materialType.equals(UNKNOWN_MATERIAL_TYPE)) {
                    nonUnknownMaterialTypeURLs.add(link);
                }
            }
        }

        // Remove duplicates while favouring SWB and, if not present, DNB links:
        for (final String material_type : materialTypeToURLsMap.keySet()) {
            if (material_type.equals(UNKNOWN_MATERIAL_TYPE)) {
                for (final String url : materialTypeToURLsMap.get(material_type)) {
                    if (!nonUnknownMaterialTypeURLs.contains(url)) {
                        urls_and_material_types.add(url + ":" + UNKNOWN_MATERIAL_TYPE);
                    }
                }
            } else {
                // Locate SWB and DNB URLs, if present:
                String preferredURL = null;
                for (final String url : materialTypeToURLsMap.get(material_type)) {
                    if (url.startsWith("http://swbplus.bsz-bw.de")) {
                        preferredURL = url;
                        break;
                    } else if (url.startsWith("http://d-nb.info"))
                        preferredURL = url;
                }

                if (preferredURL != null)
                    urls_and_material_types.add(preferredURL + ":" + material_type);
                else { // Add the kitchen sink.
                    for (final String url : materialTypeToURLsMap.get(material_type))
                        urls_and_material_types.add(url + ":" + material_type);
                }
            }
        }

        // Handle DOI's:
        for (final VariableField variableField : record.getVariableFields("024")) {
            final DataField field = (DataField) variableField;
            final Subfield subfield_2 = field.getSubfield('2');
            if (subfield_2 != null && subfield_2.getData().equals("doi")) {
                final Subfield subfield_a = field.getSubfield('a');
                if (subfield_a != null) {
                    final String url = "https://doi.org/" + subfield_a.getData();
                    urls_and_material_types.add(url + ":DOI");
                }
            }
        }

        return urls_and_material_types;
    }

    /**
     * Returns a Set<String> of parent (ID + colon + parent title + optional volume). Only
     * ID's w/o titles will not be returned.
     *
     * @param record
     *            the record
     * @return A, possibly empty, Set<String> containing the ID/title(/volume) pairs and triples.
     */
    public Set<String> getContainerIdsWithTitles(final Record record) {
        final Set<String> containerIdsTitlesAndOptionalVolumes = new TreeSet<>();

        for (final String tag : new String[] { "800", "810", "830", "773", "776" }) {
            for (final VariableField variableField : record.getVariableFields(tag)) {
                final DataField field = (DataField) variableField;
                final Subfield titleSubfield = getFirstNonEmptySubfield(field, 't', 'a');
                final Subfield volumeSubfield = field.getSubfield('v');
                final Subfield idSubfield = field.getSubfield('w');

                if (titleSubfield == null || idSubfield == null)
                    continue;

                final Matcher matcher = PPN_EXTRACTION_PATTERN.matcher(idSubfield.getData());
                if (!matcher.matches())
                    continue;
                final String parentId = matcher.group(1);

                containerIdsTitlesAndOptionalVolumes
                        .add(parentId + (char) 0x1F + titleSubfield.getData()
                             + (char) 0x1F + (volumeSubfield == null ? "" : volumeSubfield.getData()));
            }
        }
        return containerIdsTitlesAndOptionalVolumes;
    }

    private void collectReviewsAndReviewedRecords(final Record record) {
        if (reviews_cache != null && reviewedRecords_cache != null) {
            return;
        }

        reviews_cache = new TreeSet<>();
        reviewedRecords_cache = new TreeSet<>();
        for (final VariableField variableField : record.getVariableFields("787")) {
            final DataField field = (DataField) variableField;
            final Subfield reviewTypeSubfield = getFirstNonEmptySubfield(field, 'i');
            final Subfield titleSubfield = getFirstNonEmptySubfield(field, 't');
            if (titleSubfield == null || reviewTypeSubfield == null)
                continue;

            String title = titleSubfield.getData();
            final Subfield locationAndPublisher = getFirstNonEmptySubfield(field, 'd');
            if (locationAndPublisher != null)
                title = title + " (" + locationAndPublisher.getData() + ")";

            String parentId = "000000000";
            final Subfield idSubfield = field.getSubfield('w');
            if (idSubfield != null) {
                final Matcher matcher = PPN_EXTRACTION_PATTERN.matcher(idSubfield.getData());
                if (matcher.matches())
                    parentId = matcher.group(1);
            }

            final Subfield reviewerSubfield = getFirstNonEmptySubfield(field, 'a');
            final String reviewer = (reviewerSubfield == null) ? "" : reviewerSubfield.getData();

            if (reviewTypeSubfield.getData().equals("Rezension")) {
                reviews_cache.add(parentId + (char) 0x1F + reviewerSubfield.getData() + (char) 0x1F + title);
            } else if (reviewTypeSubfield.getData().equals("Rezension von")) {
                reviewedRecords_cache.add(parentId + (char) 0x1F + reviewer + (char) 0x1F + title);
            }
        }
    }

    public Set<String> getReviews(final Record record) {
        collectReviewsAndReviewedRecords(record);
        return reviews_cache;
    }

    public Set<String> getReviewedRecords(final Record record) {
        collectReviewsAndReviewedRecords(record);
        return reviewedRecords_cache;
    }

    /**
     * @param record
     *            the record
     * @param fieldnums
     * @return
     */
    public Set<String> getSuperMP(final Record record, final String fieldnums) {
        final Set<String> retval = new LinkedHashSet<>();
        final HashMap<String, String> resvalues = new HashMap<>();
        final HashMap<String, Integer> resscores = new HashMap<>();

        String value;
        String id;
        Integer score;
        Integer cscore;
        String fnum;
        String fsfc;

        final String[] fields = fieldnums.split(":");
        for (final String field : fields) {

            fnum = field.replaceAll("[a-z]+$", "");
            fsfc = field.replaceAll("^[0-9]+", "");

            final List<VariableField> fs = record.getVariableFields(fnum);
            if (fs == null) {
                continue;
            }
            for (final VariableField variableField : fs) {
                final DataField dataField = (DataField) variableField;
                final Subfield subfieldW = dataField.getSubfield('w');
                if (subfieldW == null) {
                    continue;
                }
                final Subfield fsubany = dataField.getSubfield(fsfc.charAt(0));
                if (fsubany == null) {
                    continue;
                }
                value = fsubany.getData().trim();
                id = subfieldW.getData().replaceAll("^\\([^\\)]+\\)", "");

                // Count number of commas in "value":
                score = value.length() - value.replace(",", "").length();

                if (resvalues.containsKey(id)) {
                    cscore = resscores.get(id);
                    if (cscore > score) {
                        continue;
                    }
                }
                resvalues.put(id, value);
                resscores.put(id, score);
            }
        }

        for (final String key : resvalues.keySet()) {
            value = "(" + key + ")" + resvalues.get(key);
            retval.add(value);
        }

        return retval;
    }

    /**
     * get the ISILs from LOK-tagged fields
     * <p/>
     * Typical LOK-Section below a Marc21 - Title-Set of a record: LOK |0 000
     * xxxxxnu a22 zn 4500 LOK |0 001 000001376 LOK |0 003 DE-576 LOK |0 004
     * 000000140 LOK |0 005 20020725000000 LOK |0 008
     * 020725||||||||||||||||ger||||||| LOK |0 014 |a 000001368 |b DE-576 LOK |0
     * 541 |e 76.6176 LOK |0 852 |a DE-Sp3 LOK |0 852 1 |c B IV 529 |9 00
     * <p/>
     * LOK = Field |0 852 = Subfield |a DE-Sp3 = Subfield with ISIL
     *
     * @param record
     *            the record
     * @return Set of isils
     */
    public Set<String> getIsils(final Record record) {
        if (isils_cache != null) {
            return isils_cache;
        }

        final Set<String> isils = new LinkedHashSet<>();
        final List<VariableField> fields = record.getVariableFields("LOK");
        if (fields != null) {
            for (final VariableField variableField : fields) {
                final DataField lokfield = (DataField) variableField;
                final Subfield subfield0 = lokfield.getSubfield('0');
                if (subfield0 == null || !subfield0.getData().startsWith("852")) {
                    continue;
                }
                final Subfield subfieldA = lokfield.getSubfield('a');
                if (subfieldA != null) {
                    isils.add(subfieldA.getData());
                }
            }
        }

        if (isils.isEmpty()) { // Nothing worked!
            isils.add("Unknown");
        }
        this.isils_cache = isils;
        return isils;
    }

    /**
     * @param record
     *            the record
     * @return
     */
    public String isAvailableInTuebingen(final Record record) {
        return Boolean.toString(!record.getVariableFields("SIG").isEmpty());
    }

    /**
     * get the collections from LOK-tagged fields
     * <p/>
     * Typical LOK-Section below a Marc21 - Title-Set of a record: LOK |0 000
     * xxxxxnu a22 zn 4500 LOK |0 001 000001376 LOK |0 003 DE-576 LOK |0 004
     * 000000140 LOK |0 005 20020725000000 LOK |0 008
     * 020725||||||||||||||||ger||||||| LOK |0 014 |a 000001368 |b DE-576 LOK |0
     * 541 |e 76.6176 LOK |0 852 |a DE-Sp3 LOK |0 852 1 |c B IV 529 |9 00
     * <p/>
     * LOK = Field |0 852 = Subfield |a DE-Sp3 = Subfield with ISIL
     *
     * @param record
     *            the record
     * @return Set of collections
     */
    public Set<String> getCollections(final Record record) {
        final Set<String> isils = getIsils(record);
        final Set<String> collections = new HashSet<>();
        for (final String isil : isils) {
            final String collection = isil_to_department_map.get(isil);
            if (collection != null) {
                collections.add(collection);
            } else {
                throw new IllegalArgumentException("Unknown ISIL: " + isil);
            }
        }

        if (collections.isEmpty())
            collections.add("Unknown");

        return collections;
    }

    /**
     * @param record
     *            the record
     */
    public String getInstitution(final Record record) {
        final Set<String> collections = getCollections(record);
        return collections.iterator().next();
    }

    private static boolean isValidMonthCode(final String month_candidate) {
        try {
            final int month_code = Integer.parseInt(month_candidate);
            return month_code >= 1 && month_code <= 12;
        } catch (NumberFormatException e) {
            return false;
        }
    }
    
    /**
     * @param record
     *            the record
     */
    public String getTueLocalIndexedDate(final Record record) {
        for (final VariableField variableField : record.getVariableFields("LOK")) {
            final DataField lokfield = (DataField) variableField;
            final List<Subfield> subfields = lokfield.getSubfields();
            final Iterator<Subfield> subfieldsIter = subfields.iterator();
            while (subfieldsIter.hasNext()) {
                Subfield subfield = subfieldsIter.next();
                char formatCode = subfield.getCode();

                String dataString = subfield.getData();
                if (formatCode != '0' || !dataString.startsWith("938") || !subfieldsIter.hasNext()) {
                    continue;
                }

                subfield = subfieldsIter.next();
                formatCode = subfield.getCode();
                if (formatCode != 'a') {
                    continue;
                }

                dataString = subfield.getData();
                if (dataString.length() != 4) {
                    continue;
                }

                final String sub_year_text = dataString.substring(0, 2);
                final int sub_year = Integer.parseInt("20" + sub_year_text);
                final int current_year = Calendar.getInstance().get(Calendar.YEAR);
                final String year;
                if (sub_year > current_year) {
                    // It is from the last century
                    year = "19" + sub_year_text;
                } else {
                    year = "20" + sub_year_text;
                }

                final String month = dataString.substring(2, 4);
                if (!isValidMonthCode(month)) {
                    System.err.println("in getTueLocalIndexedDate: bad month in LOK 988 field: " + month
                                       + "! (PPN: " + record.getControlNumber() + ")");
                    return null;
                }
                return year + "-" + month + "-01T11:00:00.000Z";
            }
        }
        return null;
    }

    /*
     * translation map cache
     */
    static private Map<String, String> translation_map_en = new HashMap<String, String>();
    static private Map<String, String> translation_map_fr = new HashMap<String, String>();
    static private Map<String, String> translation_map_it = new HashMap<String, String>();
    static private Map<String, String> translation_map_es = new HashMap<String, String>();
    static private Map<String, String> translation_map_hant = new HashMap<String, String>();
    static private Map<String, String> translation_map_hans = new HashMap<String, String>();

    /**
     * get translation map for normdata translations
     *
     * either get from cache or load from file, if cache empty
     *
     * @param langShortcut
     *
     * @return Map<String, String>
     * @throws IllegalArgumentException
     */
    static public Map<String, String> getTranslationMap(final String langShortcut) throws IllegalArgumentException {
        Map<String, String> translation_map;

        switch (langShortcut) {
        case "en":
            translation_map = translation_map_en;
            break;
        case "fr":
            translation_map = translation_map_fr;
            break;
        case "it":
            translation_map = translation_map_it;
            break;
        case "es":
            translation_map = translation_map_es;
            break;
        case "hant":
            translation_map = translation_map_hant;
            break;
        case "hans":
            translation_map = translation_map_hans;
            break;
        default:
            throw new IllegalArgumentException("Invalid language shortcut: " + langShortcut);
        }

        final String dir = "/usr/local/ub_tools/bsz_daten/";
        final String ext = "txt";
        final String basename = "normdata_translations";
        String translationsFilename = dir + basename + "_" + langShortcut + "." + ext;

        // Only read the data from file if necessary
        if (translation_map.isEmpty() && (new File(translationsFilename).length() != 0))  {

            try {
                BufferedReader in = new BufferedReader(new FileReader(translationsFilename));
                String line;

                while ((line = in.readLine()) != null) {
                    String[] translations = line.split("\\|");
                    if (!translations[0].isEmpty() && !translations[1].isEmpty())
                        translation_map.put(translations[0], translations[1]);
                }
            } catch (IOException e) {
                System.err.println("Could not open file: " + e.toString());
                System.exit(1);
            }
        }

        return translation_map;
    }

    /**
     * translate a string
     *
     * @param string        string to translate
     * @param langShortcut  language code
     *
     * @return              translated string if available, else input string
     */
    static public String getTranslation(final String string, final String langShortcut) {
        if (langShortcut.equals("de")) {
            return string;
        }

        Map<String, String> translationMap = getTranslationMap(langShortcut);
        final String translatedString = translationMap.get(string);
        if (translatedString != null) {
            return translatedString;
        } else {
            return string;
        }
    }

    // Returns the contents of the first data field with tag "tag" and subfield code "subfield_code" or null if no
    // such field and subfield were found.
    static private String getFirstSubfieldValue(final Record record, final String tag, final char subfieldCode) {
        if (tag == null || tag.length() != 3)
            throw new IllegalArgumentException("bad tag (null or length != 3)!");

        for (final VariableField variableField : record.getVariableFields(tag)) {
            final DataField dataField = (DataField) variableField;
            final Subfield subfield = dataField.getSubfield(subfieldCode);
            if (subfield != null)
                return subfield.getData();
        }

        return null;
    }

    /**
     * @param record
     *            the record
     */
    public String getPageRange(final Record record) {
        final String field_value = getFirstSubfieldValue(record, "936", 'h');
        if (field_value == null)
            return null;

        final Matcher matcher1 = PAGE_RANGE_PATTERN1.matcher(field_value);
        if (matcher1.matches())
            return matcher1.group(1) + "-" + matcher1.group(2);

        final Matcher matcher2 = PAGE_RANGE_PATTERN2.matcher(field_value);
        if (matcher2.matches())
            return matcher2.group(1) + "-" + matcher2.group(2);

        final Matcher matcher3 = PAGE_RANGE_PATTERN3.matcher(field_value);
        if (matcher3.matches())
            return matcher3.group(1) + "-";

        return null;
    }

    /**
     * Returns a Set<String> of Persistent Identifiers, e.g. DOIs and URNs
     * e.g.
     *  DOI:<doi1>
     *  URN:<urn1>
     *  URN:<urn2>
     * URLs are scanned for URNs from 856$u. "urn:" will be part of the URN.
     * Furthermore 024$2 will be checked for "doi".
     */
    public Set<String> getTypesAndPersistentIdentifiers(final Record record) {
        final Set<String> result = new TreeSet<>();

        // Handle DOIs
        for (final VariableField variableField : record.getVariableFields("024")) {
            final DataField field = (DataField) variableField;
            final Subfield subfield_2 = field.getSubfield('2');
            if (subfield_2 != null && subfield_2.getData().equals("doi")) {
                final Subfield subfield_a = field.getSubfield('a');
                if (subfield_a != null) {
                    result.add("DOI:" + subfield_a.getData());
                }
            }
        }

        // Handle URNs
        for (final VariableField variableField : record.getVariableFields("856")) {
            final DataField field = (DataField) variableField;

            for (final Subfield subfield_u : field.getSubfields('u')) {
                final String rawLink = subfield_u.getData();
                final int index = rawLink.indexOf("urn:", 0);

                if (index >= 0) {
                    final String link = rawLink.substring(index);
                    result.add("URN:" + link);
                }
            }
        }

        return result;
    }

    /**
     * @param record
     *            the record
     */
    public String getContainerYear(final Record record) {
        final String field_value = getFirstSubfieldValue(record, "936", 'j');
        if (field_value == null)
            return null;

        final Matcher matcher = YEAR_PATTERN.matcher(field_value);
        return matcher.matches() ? matcher.group(1) : null;
    }

    /**
     * @param record
     *            the record
     */
    public String getContainerVolume(final Record record) {
        final String field_value = getFirstSubfieldValue(record, "936", 'd');
        if (field_value == null)
            return null;

        final Matcher matcher = VOLUME_PATTERN.matcher(field_value);
        return matcher.matches() ? matcher.group(1) : null;
    }

    public Set<String> map935b(final Record record, final Map<String, String> map) {
        final Set<String> results = new TreeSet<>();
        for (final DataField data_field : record.getDataFields()) {
            if (!data_field.getTag().equals("935"))
                continue;

            final List<Subfield> physical_code_subfields = data_field.getSubfields('b');
            for (final Subfield physical_code_subfield : physical_code_subfields) {
                final String physical_code = physical_code_subfield.getData();
                if (map.containsKey(physical_code))
                    results.add(map.get(physical_code));
                else
                    System.err.println("in TuelibMixin.getPhysicalType: can't map \"" + physical_code + "\"!");
            }
        }

        return results;
    }

    /**
     * @param record
     *            the record
     */
    public Set<String> getPhysicalType(final Record record) {
        return map935b(record, TuelibMixin.phys_code_to_full_name_map);
    }

    // Removes any non-letters from "original_role".
    private static String cleanRole(final String original_role) {
        final StringBuilder canonised_role = new StringBuilder();
        for (final char ch : original_role.toCharArray()) {
            if (Character.isLetter(ch))
                canonised_role.append(ch);
        }

        return canonised_role.toString();
    }

    private static final char[] author2SubfieldCodes = new char[] { 'a', 'b', 'c', 'd' };

    private boolean isHonoree(final List<Subfield> subfieldFields4) {
        for (final Subfield subfield4 : subfieldFields4) {
            if (subfield4.getData().equals("hnr"))
                return true;
        }

        return false;
    }

    /**
     * @param record
     *            the record
     */
    public Set<String> getAuthors2AndRoles(final Record record) {
        final Set<String> results = new TreeSet<>();
        for (final VariableField variableField : record.getVariableFields("700")) {
            final DataField dataField = (DataField) variableField;

            String author2 = null;
            for (char subfieldCode : author2SubfieldCodes) {
                final Subfield subfieldField = dataField.getSubfield(subfieldCode);
                if (subfieldField != null) {
                    author2 = subfieldField.getData();
                    break;
                }
            }
            if (author2 == null)
                continue;

            final List<Subfield> _4Subfields = dataField.getSubfields('4');
            if (_4Subfields == null || _4Subfields.isEmpty() || isHonoree(_4Subfields))
                continue;

            final StringBuilder author2AndRoles = new StringBuilder();
            author2AndRoles.append(author2.replace("$", ""));
            for (final Subfield _4Subfield : _4Subfields) {
                author2AndRoles.append('$');
                author2AndRoles.append(cleanRole(_4Subfield.getData()));
            }
            results.add(author2AndRoles.toString());
        }

        return results;
    }

    private Set<String> addHonourees(final Record record, final Set<String> values) {
        for (final VariableField variableField : record.getVariableFields("700")) {
            final DataField dataField = (DataField) variableField;
            final List<Subfield> subfieldFields4 = dataField.getSubfields('4');
            if (subfieldFields4 != null) {
                for (final Subfield subfield4 : subfieldFields4) {
                    if (subfield4.getData().equals("hnr")) {
                        final List<Subfield> subfieldsA = dataField.getSubfields('a');
                        if (subfieldsA != null) {
                            for (final Subfield subfieldA : subfieldsA)
                                values.add(subfieldA.getData());
                        }
                        break;
                    }
                }
            }
        }

        return values;
    }

    public Set<String> getValuesOrUnassigned(final Record record, final String fieldSpecs) {
        final Set<String> values = SolrIndexer.instance().getFieldList(record, fieldSpecs);
        if (values.isEmpty()) {
            values.add(UNASSIGNED_STRING);
        }
        return values;
    }

    /**
     * Parse the field specifications
     */
    private Map<String, String> parseTopicSeparators(String separatorSpec) {
        final Map<String, String> separators = new HashMap<String, String>();

        // Split the string at unescaped ":"
        // See
        // http://stackoverflow.com/questions/18677762/handling-delimiter-with-escape-characters-in-java-string-split-method
        // (20160416)

        final String fieldDelim = ":";
        final String subfieldDelim = "$";
        final String esc = "\\";
        final String regexColon = "(?<!" + Pattern.quote(esc) + ")" + Pattern.quote(fieldDelim);
        final String regexSubfield = "(?<!" + Pattern.quote(esc) + ")" + Pattern.quote(subfieldDelim) + "([a-zA-Z])(.*)";
        final Pattern SUBFIELD_PATTERN = Pattern.compile(regexSubfield);

        String[] subfieldSeparatorList = separatorSpec.split(regexColon);
        for (String s : subfieldSeparatorList) {
            // System.out.println("separator Spec: " + s);
            // Create map of subfields and separators

            Matcher subfieldMatcher = SUBFIELD_PATTERN.matcher(s);

            // Extract the subfield
            if (subfieldMatcher.find()) {
                // Get $ and the character
                String subfield = subfieldMatcher.group(1);
                String separatorToUse = subfieldMatcher.group(2);
                // System.out.println("Inserting separators | subfield: " +
                // subfield + " - text: " + separatorToUse);
                separators.put(subfield, separatorToUse.replace(esc, ""));
            }
            // Use an expression that does not specify a subfield as default
            // value
            else {
                separators.put("default", s.replace(esc, ""));
            }
        }

        return separators;
    }

    private final static Pattern NUMBER_END_PATTERN = Pattern.compile("([^\\d\\s<>]+)(\\s*<?\\d+(-\\d+)>?$)");

    /**
     * Translate a single term to given language if a translation is found
     */
    public String translateTopic(String topic, String langShortcut) {
        if (langShortcut.equals("de"))
            return topic;

        Map<String, String> translation_map = TuelibMixin.getTranslationMap(langShortcut);
        Matcher numberEndMatcher = NUMBER_END_PATTERN.matcher(topic);

        // Some terms contain slash separated subterms, see whether we can
        // translate them
        if (topic.contains("\\/")) {
            String[] subtopics = topic.split("\\\\/");
            int i = 0;
            for (String subtopic : subtopics) {
                subtopics[i] = (translation_map.get(subtopic) != null) ? translation_map.get(subtopic) : subtopic;
                ++i;
            }
            topic = Utils.join(new HashSet<String>(Arrays.asList(subtopics)), "/");
        }
        // If we have a topic and a following number, try to separate the word and join it afterwards
        // This is especially important for time informations where we provide special treatment
        else if (numberEndMatcher.find()) {
            String topicText = numberEndMatcher.group(1);
            String numberExtension = numberEndMatcher.group(2);
            if (topicText.equals("Geschichte")) {
                switch (langShortcut) {
                case "en":
                    topic = "History" + numberExtension;
                    break;
                case "fr":
                    topic = "Histoire" + numberExtension;
                    break;
                }
            } else
                topic = translation_map.get(topicText) != null ? translation_map.get(topicText) + numberExtension
                                                               : topic;
        } else
            topic = (translation_map.get(topic) != null) ? translation_map.get(topic) : topic;

        return topic;
    }

    /**
     * Generate Separator according to specification
     */
    public String getSubfieldBasedSeparator(Map<String, String> separators, char subfieldCodeChar) {
        String subfieldCodeString = Character.toString(subfieldCodeChar);
        String separator = separators.get(subfieldCodeString) != null ? separators.get(subfieldCodeString)
                                                                      : separators.get("default");

        return separator;
    }

    /**
     * Abstraction for iterating over the subfields
     */

    /**
     * Generic function for topics that abstracts from a set or list collector
     * It is based on original SolrIndex.getAllSubfieldsCollector but allows to
     * specify several different separators to concatenate the single subfields
     * Moreover Numeric subfields are filtered our since the do not contain data
     * to be displayed. Separators can be defined on a subfield basis as list in
     * the format separator_spec :== separator | subfield_separator_list
     * subfield_separator_list :== subfield_separator_spec |
     * subfield_separator_spec ":" subfield_separator_list |
     * subfield_separator_spec ":" separator subfield_separator_spec :==
     * subfield_spec separator subfield_spec :== "$" character_subfield
     * character_subfield :== A character subfield (e.g. p,n,t,x...) separator
     * :== separator_without_control_characters+ | separator "\:" separator |
     * separator "\$" separator separator_without_control_characters := All
     * characters without ":" and "$" | empty_string
     */
    private void getTopicsCollector(final Record record, String fieldSpec, Map<String, String> separators,
                                    Collection<String> collector, String langShortcut)
    {
        String[] fldTags = fieldSpec.split(":");
        String fldTag;
        String subfldTags;
        List<VariableField> marcFieldList;

        for (int i = 0; i < fldTags.length; i++) {
            // Check to ensure tag length is at least 3 characters
            if (fldTags[i].length() < 3) {
                continue;
            }

            // Handle "Lokaldaten" appropriately
            if (fldTags[i].substring(0, 3).equals("LOK")) {

                if (fldTags[i].substring(3, 6).length() < 3) {
                    System.err.println("Invalid tag for \"Lokaldaten\": " + fldTags[i]);
                    continue;
                }
                // Save LOK-Subfield
                // Currently we do not support specifying an indicator
                fldTag = fldTags[i].substring(0, 6);
                subfldTags = fldTags[i].substring(6);
            } else {
                fldTag = fldTags[i].substring(0, 3);
                subfldTags = fldTags[i].substring(3);
            }

            // Case 1: We have a LOK-Field
            if (fldTag.startsWith("LOK")) {
                // Get subfield 0 since the "subtag" is saved here
                marcFieldList = record.getVariableFields("LOK");
                if (!marcFieldList.isEmpty()) {
                    for (VariableField vf : marcFieldList) {
                        DataField marcField = (DataField) vf;
                        StringBuffer buffer = new StringBuffer("");
                        Subfield subfield0 = marcField.getSubfield('0');
                        if (subfield0 == null || !subfield0.getData().startsWith(fldTag.substring(3, 6))) {
                            continue;
                        }
                        // Iterate over all given subfield codes
                        Pattern subfieldPattern = Pattern.compile(subfldTags.length() == 0 ? "." : subfldTags);
                        List<Subfield> subfields = marcField.getSubfields();
                        // Case 1: The separator specification is empty thus we
                        // add the subfields individually
                        if (separators.get("default").equals("")) {
                            for (Subfield subfield : subfields) {
                                if (Character.isDigit(subfield.getCode()))
                                    continue;
                                String term = subfield.getData().trim();
                                if (term.length() > 0)
                                    // Escape slashes in single topics since
                                    // they interfere with KWCs
                                    collector.add(translateTopic(DataUtil.cleanData(term.replace("/", "\\/")),
                                                                 langShortcut));
                            }
                        }
                        // Case 2: Generate a complex string using the
                        // separators
                        else {
                            for (Subfield subfield : subfields) {
                                // Skip numeric fields
                                if (Character.isDigit(subfield.getCode()))
                                    continue;
                                Matcher matcher = subfieldPattern.matcher("" + subfield.getCode());
                                if (matcher.matches()) {
                                    if (buffer.length() > 0) {
                                        String separator = getSubfieldBasedSeparator(separators, subfield.getCode());
                                        if (separator != null) {
                                            buffer.append(separator);
                                        }
                                    }
                                    String term = subfield.getData().trim();
                                    buffer.append(translateTopic(term.replace("/", "\\/"), langShortcut));
                                }
                            }
                            if (buffer.length() > 0)
                                collector.add(DataUtil.cleanData(buffer.toString()));
                        }
                    }
                }
            }

            // Case 2: We have an ordinary MARC field
            else {
                marcFieldList = record.getVariableFields(fldTag);
                if (!marcFieldList.isEmpty()) {
                    Pattern subfieldPattern = Pattern.compile(subfldTags.length() == 0 ? "." : subfldTags);
                    for (VariableField vf : marcFieldList) {
                        DataField marcField = (DataField) vf;
                        StringBuffer buffer = new StringBuffer("");
                        List<Subfield> subfields = marcField.getSubfields();
                        // Case 1: The separator specification is empty thus we
                        // add the subfields individually
                        if (separators.get("default").equals("")) {
                            for (Subfield subfield : subfields) {
                                if (Character.isDigit(subfield.getCode()))
                                    continue;
                                String term = subfield.getData().trim();
                                if (term.length() > 0)
                                    collector.add(translateTopic(DataUtil.cleanData(term.replace("/", "\\/")), langShortcut));
                            }
                        }
                        // Case 2: Generate a complex string using the
                        // separators
                        else {
                            for (Subfield subfield : subfields) {
                                // Skip numeric fields
                                if (Character.isDigit(subfield.getCode()))
                                    continue;
                                Matcher matcher = subfieldPattern.matcher("" + subfield.getCode());
                                if (matcher.matches()) {
                                    if (buffer.length() > 0) {
                                        String separator = getSubfieldBasedSeparator(separators, subfield.getCode());
                                        if (separator != null) {
                                            buffer.append(separator);
                                        }
                                    }
                                    String term = subfield.getData().trim();
                                    buffer.append(translateTopic(term.replace("/", "\\/"), langShortcut));
                                }
                            }
                            if (buffer.length() > 0)
                                collector.add(DataUtil.cleanData(buffer.toString()));
                        }
                    }
                }
            }
        }

        return;
    }

    public Set<String> getTopics(final Record record, String fieldSpec, String separator, String langShortcut)
        throws FileNotFoundException
    {
        final Set<String> topics = new HashSet<String>();
        // It seems to be a general rule that in the fields that the $p fields
        // are converted to a '.'
        // $n is converted to a space if there is additional information
        Map<String, String> separators = parseTopicSeparators(separator);
        getTopicsCollector(record, fieldSpec, separators, topics, langShortcut);
        return addHonourees(record, topics);
    }

    public Set<String> getTopicFacet(final Record record, final String fieldSpecs) {
        final Set<String> values = getValuesOrUnassigned(record, fieldSpecs);
        return addHonourees(record, values);
    }

    public Set<String> getValuesOrUnassignedTranslated(final Record record, final String fieldSpecs,
                                                       final String langShortcut)
    {
        Set<String> valuesTranslated = new TreeSet<String>();
        Set<String> values = getValuesOrUnassigned(record, fieldSpecs);
        for (final String value : values) {
            final String translatedValue = getTranslation(value, langShortcut);
            valuesTranslated.add(translatedValue);
        }
        return valuesTranslated;
    }

    public Set<String> getTopicFacetTranslated(final Record record, final String fieldSpecs, final String lang) {
        final Set<String> valuesTranslated = getValuesOrUnassignedTranslated(record, fieldSpecs, lang);
        return addHonourees(record, valuesTranslated);
    }

    public String getFirstValueOrUnassigned(final Record record, final String fieldSpecs) {
        final Set<String> values = SolrIndexer.instance().getFieldList(record, fieldSpecs);
        if (values.isEmpty()) {
            values.add(UNASSIGNED_STRING);
        }
        return values.iterator().next();
    }

    private static boolean isSerialComponentPart(final Record record) {
        final String leader = record.getLeader().toString();
        return leader.charAt(7) == 'b';
    }


    private String checkValidYear(String fourDigitYear) {
        Matcher validFourDigitYearMatcher = VALID_FOUR_DIGIT_PATTERN.matcher(fourDigitYear);
        return validFourDigitYearMatcher.matches() ? fourDigitYear : "";
    }

    private String yyMMDateToString(final String controlNumber, final String yyMMDate) {
        int currentYear = Calendar.getInstance().get(Calendar.YEAR);
        int yearTwoDigit = currentYear - 2000;  // If extraction fails later we fall back to current year
        try {
            yearTwoDigit = Integer.parseInt(yyMMDate.substring(0, 1));
        }
        catch (NumberFormatException e) {
            System.err.println("in yyMMDateToString: expected date in YYMM format, found \"" + yyMMDate
                               + "\" instead! (Control number was " + controlNumber + ")");
        }
        return Integer.toString(yearTwoDigit < (currentYear - 2000) ? (2000 + yearTwoDigit) : (1900 + yearTwoDigit));
    }

    /**
     * Get all available dates from the record.
     *
     * @param record
     *            MARC record
     * @return set of dates
     */

    public Set<String> getDates(final Record record) {
        final Set<String> dates = new LinkedHashSet<>();
        final Set<String> format = getFormatIncludingElectronic(record);

        // Case 1 [Website]
        if (format.contains("Website")) {
            final ControlField _008_field = (ControlField) record.getVariableField("008");
            if (_008_field == null) {
                System.err.println("getDates [No 008 Field for Website " + record.getControlNumber() + "]");
                return dates;
            }
            dates.add(yyMMDateToString(record.getControlNumber(), _008_field.getData()));
            return dates;
        }

        // Case 2 [Reproduction] (Reproductions have the publication date of the original work in 534$c.)
        final VariableField _534Field = record.getVariableField("534");
        if (_534Field != null) {
            final DataField dataField = (DataField) _534Field;
            final Subfield cSubfield = dataField.getSubfield('c');
            if (cSubfield != null) {
                // strip non-digits at beginning and end (e.g. "©")
                String date = cSubfield.getData();
                date = date.replaceAll("^[^0-9]+", "");
                date = date.replaceAll("[^0-9]+$", "");
                dates.add(date);
                return dates;
            }
        }

        // Case 3 [Article or Review]
        // Match also the case of publication date transgressing one year
        // (Format YYYY/YY for older and Format YYYY/YYYY) for
        // newer entries
        if (format.contains("Article") || (format.contains("Review") && !format.contains("Book"))) {
            final List<VariableField> _936Fields = record.getVariableFields("936");
            for (VariableField _936VField : _936Fields) {
                DataField _936Field = (DataField) _936VField;
                final Subfield jSubfield = _936Field.getSubfield('j');
                if (jSubfield != null) {
                    String yearOrYearRange = jSubfield.getData();
                    // Partly, we have additional text like "Post annum domini" in the front, so do away with that
                    yearOrYearRange = yearOrYearRange.replaceAll("^[\\D\\[\\]]+", "");
                    // Make sure we do away with brackets
                    yearOrYearRange = yearOrYearRange.replaceAll("[\\[|\\]]", "");
                    dates.add(yearOrYearRange.length() > 4 ? yearOrYearRange.substring(0, 4) : yearOrYearRange);
                }
            }
            if (dates.isEmpty())
                System.err.println("getDates [Could not find proper 936 field date content for: " + record.getControlNumber() + "]");
            else
                return dates;
        }

        // Case 4:
        // Test whether we have a 190j field
        // This was generated in the pipeline for superior works that do not contain a reasonable 008(7,10) entry
        final List<VariableField> _190Fields = record.getVariableFields("190");
        for (VariableField _190VField : _190Fields) {
            final DataField _190Field = (DataField) _190VField;
            final Subfield jSubfield = _190Field.getSubfield('j');
            if (jSubfield != null) {
                dates.add(jSubfield.getData());
            }
            else {
                System.err.println("getDates [No 190j subfield for PPN " + record.getControlNumber() + "]");
            }
            return dates;
        }


        // Case 5:
        // Use the sort date given in the 008-Field
        final ControlField _008_field = (ControlField) record.getVariableField("008");
        if (_008_field == null) {
            System.err.println("getDates [Could not find 008 field for PPN:" + record.getControlNumber() + "]");
            return dates;
        }
        final String _008FieldContents = _008_field.getData();
        final String yearExtracted = _008FieldContents.substring(7, 11);
        // Test whether we have a reasonable value
        final String year = checkValidYear(yearExtracted);
        if (year.isEmpty())
            System.err.println("getDates [\"" + yearExtracted + "\" is not a valid year for PPN " + record.getControlNumber() + "]");
        dates.add(year);
        return dates;
}


    public String isSuperiorWork(final Record record) {
        final DataField sprField = (DataField) record.getVariableField("SPR");
        if (sprField == null)
            return Boolean.FALSE.toString();
        return Boolean.toString(sprField.getSubfield('a') != null);
    }

    public String isSubscribable(final Record record) {
        final DataField sprField = (DataField) record.getVariableField("SPR");
        if (sprField == null)
            return Boolean.FALSE.toString();
        return Boolean.toString(sprField.getSubfield('b') != null);
    }

    private static String currentYear = null;

    /** @return the last two digits of the current year. */
    private static String getCurrentYear() {
        if (currentYear == null) {
            final DateFormat df = new SimpleDateFormat("yy");
            currentYear = df.format(Calendar.getInstance().getTime());
        }
        return currentYear;
    }

    /**
     * @brief Extracts the date (YYMMDD) that the record was created from a part
     *        of the 008 field.
     */
    public String getRecordingDate(final Record record) {
        final ControlField _008_field = (ControlField) record.getVariableField("008");
        final String fieldContents = _008_field.getData();

        final StringBuilder iso8601_date = new StringBuilder(10);
        iso8601_date.append(fieldContents.substring(0, 2).compareTo(getCurrentYear()) > 0 ? "19" : "20");
        iso8601_date.append(fieldContents.substring(0, 2));
        iso8601_date.append('-');
        iso8601_date.append(fieldContents.substring(2, 4));
        iso8601_date.append('-');
        iso8601_date.append(fieldContents.substring(4, 6));
        iso8601_date.append("T00:00:00Z");

        return iso8601_date.toString();
    }

    /**
     * @brief Extracts the date and time from the 005 field.
     */
    public String getLastModificationTime(final Record record) {
        final ControlField _005_field = (ControlField) record.getVariableField("005");
        final String fieldContents = _005_field.getData();

        final StringBuilder iso8601_date = new StringBuilder(19);
        iso8601_date.append(fieldContents.substring(0, 4));
        iso8601_date.append('-');
        iso8601_date.append(fieldContents.substring(4, 6));
        iso8601_date.append('-');
        iso8601_date.append(fieldContents.substring(6, 8));
        iso8601_date.append('T');
        iso8601_date.append(fieldContents.substring(8, 10));
        iso8601_date.append(':');
        iso8601_date.append(fieldContents.substring(10, 12));
        iso8601_date.append(':');
        iso8601_date.append(fieldContents.substring(12, 14));
        iso8601_date.append('Z');

        return iso8601_date.toString();
    }

    public Set<String> getGenreTranslated(final Record record, final String fieldSpecs, final String langShortcut) {
        final Set<String> genres = getValuesOrUnassignedTranslated(record, fieldSpecs, langShortcut);

        // Also try to find the code for "Festschrift" in 935$c:
        List<VariableField> _935Fields = record.getVariableFields("935");
        for (final VariableField _935Field : _935Fields) {
            DataField dataField = (DataField) _935Field;
            final List<Subfield> cSubfields = dataField.getSubfields('c');
            for (final Subfield cSubfield : cSubfields) {
                if (cSubfield.getData().toLowerCase().equals("fe"))
                    genres.add("Festschrift");
            }
        }

        // Also add "Formschlagwort"s from Keywordchains to genre
        List<VariableField> _689Fields = record.getVariableFields("689");
        for (final VariableField _689Field : _689Fields) {
            DataField dataField = (DataField) _689Field;
            final List<Subfield> qSubfields = dataField.getSubfields('q');
            for (final Subfield qSubfield : qSubfields) {
                if (qSubfield.getData().toLowerCase().equals("f")) {
                    final List<Subfield> aSubfields = dataField.getSubfields('a');
                    for (final Subfield aSubfield : aSubfields)
                        genres.add(aSubfield.getData());
                }
            }
        }

        if (genres.size() > 1)
            genres.remove(UNASSIGNED_STRING);

        return genres;
    }

    // Map used by getPhysicalType().
    private static final Map<String, String> phys_code_to_format_map;

    static {
        Map<String, String> tempMap = new HashMap<>();
        tempMap.put("arbtrans", "Transparency");
        tempMap.put("blindendr", "Braille");
        tempMap.put("bray", "BRDisc");
        tempMap.put("cdda", "SoundDisc");
        tempMap.put("ckop", "Microfiche");
        tempMap.put("cofz", "Online Resource");
        tempMap.put("crom", "CDROM");
        tempMap.put("dias", "Slide");
        tempMap.put("disk", "Diskette");
        tempMap.put("druck", "Printed Material");
        tempMap.put("dvda", "Audio DVD");
        tempMap.put("dvdr", "DVD-ROM");
        tempMap.put("dvdv", "Video DVD");
        tempMap.put("gegenst", "Physical Object");
        tempMap.put("handschr", "Longhand Text");
        tempMap.put("kunstbl", "Artistic Works on Paper");
        tempMap.put("lkop", "Mircofilm");
        tempMap.put("medi", "Multiple Media Types");
        tempMap.put("scha", "Record");
        tempMap.put("skop", "Microform");
        tempMap.put("sobildtt", "Audiovisual Carriers");
        tempMap.put("soerd", "Carriers of Other Electronic Data");
        tempMap.put("sott", "Carriers of Other Audiodata");
        tempMap.put("tonbd", "Audiotape");
        tempMap.put("tonks", "Audiocasette");
        tempMap.put("vika", "VideoCasette");
        phys_code_to_format_map = Collections.unmodifiableMap(tempMap);
    }

    // Set used by getPhysicalType().
    private static final Set<String> electronicResourceCarrierTypes;

    static {
        Set<String> tempSet = new HashSet<>();
        tempSet.add("cb");
        tempSet.add("cd");
        tempSet.add("ce");
        tempSet.add("ca");
        tempSet.add("cf");
        tempSet.add("ch");
        tempSet.add("cr");
        tempSet.add("ck");
        tempSet.add("cz");
        electronicResourceCarrierTypes = Collections.unmodifiableSet(tempSet);
    };

    /**
     * Determine Record Formats
     *
     * Overwrite the original VuFindIndexer getFormats to do away with the
     * single bucket approach, i.e. collect all formats you find, i.e. this is
     * the original code without premature returns which are left in commented
     * out
     *
     * @param record
     *            MARC record
     * @return set of record format
     */
    public Set<String> getMultipleFormats(final Record record) {
        final Set<String> result = map935b(record, TuelibMixin.phys_code_to_format_map);
        final String leader = record.getLeader().toString();
        final ControlField fixedField = (ControlField) record.getVariableField("008");
        final DataField title = (DataField) record.getVariableField("245");
        String formatString;
        char formatCode = ' ';
        char formatCode2 = ' ';
        char formatCode4 = ' ';

        // check if there's an h in the 245
        if (title != null) {
            if (title.getSubfield('h') != null) {
                if (title.getSubfield('h').getData().toLowerCase().contains("[electronic resource]")) {
                    result.add("Electronic");
                }
            }
        }

        // Evaluate the carrier-type field as to the object being an electronic resource or not:
        List<VariableField> _338_fields = record.getVariableFields("338");
        for (final VariableField _338_field : _338_fields) {
            final DataField dataField = (DataField) _338_field;
            if (dataField.getSubfield('b') != null
                && TuelibMixin.electronicResourceCarrierTypes.contains(dataField.getSubfield('b').getData()))
                result.add("Electronic");
        }

        // check the 007 - this is a repeating field
        List<VariableField> fields = record.getVariableFields("007");
        if (fields != null) {
            ControlField formatField;
            for (final VariableField varField : fields) {
                formatField = (ControlField) varField;
                formatString = formatField.getData().toUpperCase();
                formatCode = formatString.length() > 0 ? formatString.charAt(0) : ' ';
                formatCode2 = formatString.length() > 1 ? formatString.charAt(1) : ' ';
                formatCode4 = formatString.length() > 4 ? formatString.charAt(4) : ' ';
                switch (formatCode) {
                case 'A':
                    switch (formatCode2) {
                    case 'D':
                        result.add("Atlas");
                        break;
                    default:
                        result.add("Map");
                        break;
                    }
                    break;
                case 'C':
                    switch (formatCode2) {
                    case 'A':
                        result.add("TapeCartridge");
                        break;
                    case 'B':
                        result.add("ChipCartridge");
                        break;
                    case 'C':
                        result.add("DiscCartridge");
                        break;
                    case 'F':
                        result.add("TapeCassette");
                        break;
                    case 'H':
                        result.add("TapeReel");
                        break;
                    case 'J':
                        result.add("FloppyDisk");
                        break;
                    case 'M':
                    case 'O':
                        result.add("CDROM");
                        break;
                    case 'R':
                        // Do not return - this will cause anything with an
                        // 856 field to be labeled as "Electronic"
                        break;
                    default:
                        result.add("Software");
                        break;
                    }
                    break;
                case 'D':
                    result.add("Globe");
                    break;
                case 'F':
                    result.add("Braille");
                    break;
                case 'G':
                    switch (formatCode2) {
                    case 'C':
                    case 'D':
                        result.add("Filmstrip");
                        break;
                    case 'T':
                        result.add("Transparency");
                        break;
                    default:
                        result.add("Slide");
                        break;
                    }
                    break;
                case 'H':
                    result.add("Microfilm");
                    break;
                case 'K':
                    switch (formatCode2) {
                    case 'C':
                        result.add("Collage");
                        break;
                    case 'D':
                        result.add("Drawing");
                        break;
                    case 'E':
                        result.add("Painting");
                        break;
                    case 'F':
                        result.add("Print");
                        break;
                    case 'G':
                        result.add("Photonegative");
                        break;
                    case 'J':
                        result.add("Print");
                        break;
                    case 'L':
                        result.add("Drawing");
                        break;
                    case 'O':
                        result.add("FlashCard");
                        break;
                    case 'N':
                        result.add("Chart");
                        break;
                    default:
                        result.add("Photo");
                        break;
                    }
                    break;
                case 'M':
                    switch (formatCode2) {
                    case 'F':
                        result.add("VideoCassette");
                        break;
                    case 'R':
                        result.add("Filmstrip");
                        break;
                    default:
                        result.add("MotionPicture");
                        break;
                    }
                    break;
                case 'O':
                    result.add("Kit");
                    break;
                case 'Q':
                    result.add("MusicalScore");
                    break;
                case 'R':
                    result.add("SensorImage");
                    break;
                case 'S':
                    switch (formatCode2) {
                    case 'D':
                        result.add("SoundDisc");
                        break;
                    case 'S':
                        result.add("SoundCassette");
                        break;
                    default:
                        result.add("SoundRecording");
                        break;
                    }
                    break;
                case 'V':
                    switch (formatCode2) {
                    case 'C':
                        result.add("VideoCartridge");
                        break;
                    case 'D':
                        switch (formatCode4) {
                        case 'S':
                            result.add("BRDisc");
                            break;
                        case 'V':
                        default:
                            result.add("VideoDisc");
                            break;
                        }
                        break;
                    case 'F':
                        result.add("VideoCassette");
                        break;
                    case 'R':
                        result.add("VideoReel");
                        break;
                    default:
                        result.add("Video");
                        break;
                    }
                    break;
                }
            }
        }
        // check the Leader at position 6
        char leaderBit = leader.charAt(6);
        switch (Character.toUpperCase(leaderBit)) {
        case 'C':
        case 'D':
            result.add("MusicalScore");
            break;
        case 'E':
        case 'F':
            result.add("Map");
            break;
        case 'G':
            result.add("Slide");
            break;
        case 'I':
            result.add("SoundRecording");
            break;
        case 'J':
            result.add("MusicRecording");
            break;
        case 'K':
            result.add("Photo");
            break;
        case 'M':
            result.add("Electronic");
            break;
        case 'O':
        case 'P':
            result.add("Kit");
            break;
        case 'R':
            result.add("PhysicalObject");
            break;
        case 'T':
            result.add("Manuscript");
            break;
        }

        // check the Leader at position 7
        leaderBit = leader.charAt(7);
        switch (Character.toUpperCase(leaderBit)) {
        // Monograph
        case 'M':
            if (formatCode == 'C') {
                result.add("eBook");
            } else {
                result.add("Book");
            }
            break;
        // Component parts
        case 'A':
            result.add("BookComponentPart");
            break;
        case 'B':
            result.add("SerialComponentPart");
            break;
        // Serial
        case 'S':
            // Look in 008 to determine what type of Continuing Resource
            formatCode = fixedField.getData().toUpperCase().charAt(21);
            switch (formatCode) {
            case 'N':
                result.add("Newspaper");
                break;
            case 'P':
                result.add("Journal");
                break;
            default:
                result.add("Serial");
                break;
            }
        }

        // Records that contain the code "sodr" in 935$c should be classified as "Article" and not as "Book":
        if (!result.contains("Article")) {
            final List<VariableField> _935Fields = record.getVariableFields("935");
            for (final VariableField variableField : _935Fields) {
                final DataField _935Field = (DataField) variableField;
                if (_935Field != null) {
                    for (final Subfield cSubfield : _935Field.getSubfields('c')) {
                        if (cSubfield.getData().equals("sodr")) {
                            result.remove("Book");
                            if (result.contains("eBook")) {
                                result.remove("eBook");
                                result.add("Electronic");
                            }
                            result.add("Article");
                            break;
                        }
                    }
                }
            }
        }

        // Nothing worked!
        if (result.isEmpty()) {
            result.add("Unknown");
        }

        return result;
    }

    /**
     * Determine Record Format(s)
     *
     * @param record
     *            the record
     * @return format of record
     */
    public Set<String> getFormatsWithGermanHandling(final Record record) {
        // We've been facing the problem that the original SolrMarc cannot deal
        // with
        // german descriptions in the 245h and thus assigns a wrong format
        // for e.g. electronic resource
        // Thus we must handle this manually

        Set<String> rawFormats = new LinkedHashSet<String>();
        DataField title = (DataField) record.getVariableField("245");

        if (title != null) {
            if (title.getSubfield('h') != null) {
                if (title.getSubfield('h').getData().toLowerCase().contains("[elektronische ressource]")) {
                    rawFormats.add("Electronic");
                    rawFormats.addAll(getMultipleFormats(record));
                    return rawFormats;
                } else {
                    return getMultipleFormats(record);
                }
            }
        }

        // Catch case of empty title
        return getMultipleFormats(record);
    }

    /**
     * Determine Record Format(s) including the electronic tag The electronic
     * category is filtered out in the actula getFormat function but needed to
     * determine the media type
     *
     * @param record
     *            the record
     * @return format of record
     */
    public Set<String> getFormatIncludingElectronic(final Record record) {
        final Set<String> formats = new HashSet<>();
        Set<String> rawFormats = getFormatsWithGermanHandling(record);

        for (final String rawFormat : rawFormats) {
            if (rawFormat.equals("BookComponentPart") || rawFormat.equals("SerialComponentPart")) {
                formats.add("Article");
            } else {
                formats.add(rawFormat);
            }
        }

        // Determine whether an article is in fact a review
        final List<VariableField> _655Fields = record.getVariableFields("655");
        for (final VariableField _655Field : _655Fields) {
            final DataField dataField = (DataField) _655Field;
            if (dataField.getIndicator1() == ' ' && dataField.getIndicator2() == '7' && dataField.getSubfield('a').getData().startsWith("Rezension")) {
                formats.remove("Article");
                formats.add("Review");
                break;
            }
            if (dataField.getSubfield('a').getData().startsWith("Weblog")) {
                formats.remove("Journal");
                formats.add("Blog");
                break;
            }
        }

        // A review can also be indicated if 935$c set to "uwre"
        final List<VariableField> _935Fields = record.getVariableFields("935");
        for (final VariableField _935Field : _935Fields) {
            final DataField dataField = (DataField) _935Field;
            final Subfield cSubfield = dataField.getSubfield('c');
            if (cSubfield != null && cSubfield.getData().contains("uwre")) {
                formats.remove("Article");
                formats.add("Review");
                break;
            }
        }

        // Determine whether record is a 'Festschrift', i.e. has "fe" in 935$c
        for (final VariableField _935Field : _935Fields) {
            final DataField dataField = (DataField) _935Field;
            final Subfield cSubfield = dataField.getSubfield('c');
            if (cSubfield != null && cSubfield.getData().contains("fe")) {
                formats.add("Festschrift");
                break;
            }
        }

        // Determine whether record is a Website, i.e. has "website" in 935$c
        for (final VariableField _935Field : _935Fields) {
            final DataField dataField = (DataField) _935Field;
            List<Subfield> subfields = dataField.getSubfields();
            boolean foundMatch = false;
            for (Subfield subfield : subfields) {
                if (subfield.getCode() == 'c' && subfield.getData().contains("website")) {
                    formats.add("Website");
                    foundMatch = true;
                    break;
                }
            }
            if (foundMatch == true)
                break;
        }

        // Determine whether a record is a database, i.e. has "daten" in 935$c
        for (final VariableField _935Field : _935Fields) {
            final DataField dataField = (DataField) _935Field;
            List<Subfield> subfields = dataField.getSubfields();
            boolean foundMatch = false;
            for (Subfield subfield : subfields) {
                if (subfield.getCode() == 'c' && subfield.getData().contains("daten")) {
                    formats.add("Database");
                    foundMatch = true;
                    break;
                }
            }
            if (foundMatch == true)
                break;
        }


        // Rewrite all E-Books as electronic Books
        if (formats.contains("eBook")) {
            formats.remove("eBook");
            formats.add("Electronic");
            formats.add("Book");
        }

        return formats;
    }

    /**
     * Determine Format(s) but do away with the electronic tag
     *
     * @param record
     *            the record
     * @return mediatype of the record
     */

    public Set<String> getFormat(final Record record) {
        Set<String> formats = getFormatIncludingElectronic(record);

        // Since we now have an additional facet mediatype we remove the
        // electronic label
        formats.remove("Electronic");

        return formats;
    }

    /**
     * Determine Mediatype For facets we need to differentiate between
     * electronic and non-electronic resources
     *
     * @param record
     *            the record
     * @return mediatype of the record
     */

    public Set<String> getMediatype(final Record record) {
        final Set<String> mediatype = new HashSet<>();
        final Set<String> formats = getFormatIncludingElectronic(record);
        final String electronicRessource = "Electronic";
        final String nonElectronicRessource = "Non-Electronic";

        if (formats.contains(electronicRessource)) {
            mediatype.add(electronicRessource);
        } else {
            mediatype.add(nonElectronicRessource);
        }

        return mediatype;
    }

    /**
     * Helper to calculate the first publication date
     *
     * @param dates
     *            String of possible publication dates
     * @return the first publication date
     */

    public String calculateFirstPublicationDate(Set<String> dates) {
        String firstPublicationDate = null;
        for (final String current : dates) {
            if (firstPublicationDate == null || current != null && Integer.parseInt(current) < Integer.parseInt(firstPublicationDate))
                firstPublicationDate = current;
        }
        return firstPublicationDate;
    }

    /**
     * Helper to calculate the most recent publication date
     *
     * @param dates
     *            String of possible publication dates
     * @return the first publication date
     */

    public String calculateLastPublicationDate(Set<String> dates) {
        String lastPublicationDate = null;
        for (final String current : dates) {
            if (lastPublicationDate == null || current != null && Integer.parseInt(current) > Integer.parseInt(lastPublicationDate))
                lastPublicationDate = current;
        }
        return lastPublicationDate;
    }

    /**
     * Helper to cope with differing dates and possible special characters
     *
     * @param dates
     *            String of possible publication dates
     * @return the first publication date
     */
    public String getCleanAndNormalizedDate(final String dateString) {
        // We have to normalize dates that follow a different calculation of
        // time, e.g. works with hindu time
        final String DIFFERENT_CALCULATION_OF_TIME_REGEX = ".*?\\[(.*?)\\=\\s*(\\d+)\\s*\\].*";
        Matcher differentCalcOfTimeMatcher = Pattern.compile(DIFFERENT_CALCULATION_OF_TIME_REGEX).matcher(dateString);
        return differentCalcOfTimeMatcher.find() ? differentCalcOfTimeMatcher.group(2) : DataUtil.cleanDate(dateString);

    }

    /**
     * Determine the publication date for "date ascending/descending" sorting in
     * accordance with the rules stated in issue 227
     *
     * @param record
     *            MARC record
     * @return the publication date to be used for
     */

    public String getPublicationSortDate(final Record record) {
        final Set<String> dates = getDates(record);
        if (dates.isEmpty())
            return "";

        return calculateLastPublicationDate(dates);
    }

    public Set<String> getRecordSelectors(final Record record) {
        final Set<String> result = new TreeSet<String>();

        for (final VariableField variableField : record.getVariableFields("LOK")) {
            final DataField lokfield = (DataField) variableField;
            final Subfield subfield_0 = lokfield.getSubfield('0');
            if (subfield_0 == null || !subfield_0.getData().equals("935  ")) {
                continue;
            }

            for (final Subfield subfield_a : lokfield.getSubfields('a')) {
                if (!subfield_a.getData().isEmpty()) {
                    result.add(subfield_a.getData());
                }
            }
        }

        return result;
    }

    public String getZDBNumber(final Record record) {
        final List<VariableField> _035Fields = record.getVariableFields("035");

        for (final VariableField _035Field : _035Fields) {
            DataField field = (DataField)_035Field;
            final Subfield subfieldA = field.getSubfield('a');
            if (subfieldA != null && subfieldA.getData().startsWith("(DE-599)ZDB"))
                return subfieldA.getData().substring(11);
        }

        return null;
    }

    public String getStartPage(final Record record) {
        final DataField _936Field = (DataField)record.getVariableField("936");
        if (_936Field == null)
           return null;
        final Subfield subfieldH = _936Field.getSubfield('h');
        if (subfieldH == null)
            return null;

        final String pages = subfieldH.getData();
        final Matcher matcher = START_PAGE_MATCH_PATTERN.matcher(pages);
        if (matcher.matches()) {
            final String start_page = matcher.group(1);
            return start_page;
        }
        return null;
    }

    /** @return "true" if we have an open access publication, else "false". */
    public String getOpenAccessStatus(final Record record) {
        for (final VariableField variableField : record.getVariableFields("856")) {
            final DataField dataField = (DataField) variableField;
            final Subfield subfieldZ = dataField.getSubfield('z');
            if (subfieldZ != null && subfieldZ.getData().toLowerCase().startsWith("kostenfrei")) {
                final Subfield subfield3 = dataField.getSubfield('3');
                if (subfield3 == null || subfield3.getData().toLowerCase().equals("volltext"))
                    return "open-access";
            }
        }

        return "non-open-access";
    }


    // Try to get a numerically sortable representation of an issue

    public String getIssueSort(final Record record) {
        for (final VariableField variableField : record.getVariableFields("936")) {
            final DataField dataField = (DataField) variableField;
            final Subfield subfieldE = dataField.getSubfield('e');
            if (subfieldE == null)
                return "0";
            final String issueString = subfieldE.getData();
            if (issueString.matches("\\d+"))
                return issueString;
            // Handle Some known special cases
            if (issueString.matches("[\\[]\\d+[\\]]"))
                return issueString.replace("[]","");
            if (issueString.matches("\\d+/\\d+"))
                return issueString.split("/")[0];
        }
        return "0";
    }
}
