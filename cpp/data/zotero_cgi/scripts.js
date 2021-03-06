function GenerateISSNSearchLink(issn) {
    return '<a href="http://www.sherpa.ac.uk/romeo/search.php?issn='+issn+'" target="_blank">' + issn + '</a>';
}

function SortSelectOptions(selector) {
    var selected_value = $(selector).val();
    $(selector).html($("option", selector).sort(function (a, b) {
        return a.text == b.text ? 0 : a.text < b.text ? -1 : 1;
    })).val(selected_value);
}

function UpdateRuntime(seconds) {
    var div_runtime = document.getElementById('runtime');
    div_runtime.innerHTML = seconds + 's';
}

function UpdateSelectionDetails() {
    // trigger change logic,
    // so fields are updated properly to selections
    $('#rss_journal_title').change();
    $('#direct_journal_title').change();
    $('#crawling_journal_title').change();
}

function OnChangeRssJournal() {
    var journal_title = $("#rss_journal_title option:selected").text();

    if (journal_title != "") {
        var journal_options = rss_options_map[journal_title];
        $("#rss_journal_print_issn").html(GenerateISSNSearchLink(journal_options.issn_print));
        $("#rss_journal_online_issn").html(GenerateISSNSearchLink(journal_options.issn_online));
        $("#rss_journal_ppn_print").html(journal_options.ppn_print);
        $("#rss_journal_ppn_online").html(journal_options.ppn_online);
        $("#rss_feed_url").html("<a href=\""+journal_options.feed_url+"\" target=\"_blank\">"+journal_options.feed_url+"</a>");
    }

    $("#rss_submit").prop("disabled", journal_title == "");
}

function OnChangeCrawlingJournal() {
    var journal_title = $("#crawling_journal_title option:selected").text();

    if (journal_title != "") {
        var journal_options = crawling_options_map[journal_title];
        $("#crawling_journal_print_issn").html(GenerateISSNSearchLink(journal_options.issn_print));
        $("#crawling_journal_online_issn").html(GenerateISSNSearchLink(journal_options.issn_online));
        $("#crawling_journal_ppn_print").html(journal_options.ppn_print);
        $("#crawling_journal_ppn_online").html(journal_options.ppn_online);
        $("#crawling_base_url").html("<a href=\""+journal_options.base_url+"\" target=\"_blank\">"+journal_options.base_url+"</a>");
        $("#crawling_extraction_regex").text(journal_options.regex);
        $("#crawling_depth").text(journal_options.depth);
    }

    $("#crawling_submit").prop("disabled", journal_title == "");
}

function RenderDataTable() {
    $('#all_journals').DataTable({
        "order": [[0, "asc"], [1, "asc"]],
        "iDisplayLength": 75
    });

    $('.issn_generate_link').each(function(index){
        var issn = $(this).text();
        if (issn != "")
            $(this).html(GenerateISSNSearchLink(issn));
    });
}

function TryRss(journal_title) {
    $('#home-rss').tab('show');
    $('#rss_journal_title').val(journal_title).change();
}

function TryCrawling(journal_title) {
    $('#home-crawling').tab('show');
    $('#crawling_journal_title').val(journal_title).change();
}

function TryUrl(journal_title) {
    $('#home-url').tab('show');
    $('#url_journal_title').val(journal_title).change();
}
