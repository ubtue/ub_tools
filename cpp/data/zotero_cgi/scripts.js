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
    $('#crawling_journal_title').change();
}

function OnChangeRssJournal() {
    var journal_title = $("#rss_journal_title option:selected").text();
    var journal_options = rss_options_map[journal_title];
    $("#rss_journal_print_issn").val(journal_options.issn_print);
    $("#rss_journal_online_issn").val(journal_options.issn_online);
    $("#rss_feed_url").val(journal_options.feed_url);
    $("#rss_strptime_format").val(journal_options.strptime_format);
}

function OnChangeCrawlingJournal() {
    var journal_title = $("#crawling_journal_title option:selected").text();
    var journal_options = crawling_options_map[journal_title];
    $("#crawling_journal_print_issn").val(journal_options.issn_print);
    $("#crawling_journal_online_issn").val(journal_options.issn_online);
    $("#crawling_base_url").val(journal_options.base_url);
    $("#crawling_extraction_regex").val(journal_options.regex);
    $("#crawling_depth").val(journal_options.depth);
    $("#crawling_strptime_format").val(journal_options.strptime_format);
}

function TryRss(rss_journal_title) {
    $('#home-rss').tab('show');
    $('#rss_journal_title').val(rss_journal_title).change();
}

function TryCrawling(crawling_journal_title) {
    $('#home-crawling').tab('show');
    $('#crawling_journal_title').val(crawling_journal_title).change();
}

$(document).ready(function() {
    $('#all_journals').DataTable({
        "order": [[0, "asc"]],
        "iDisplayLength": 25
    });
});
