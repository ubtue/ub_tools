#include <iostream>
#include <cstdlib>
#include "EmailSender.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " sender recipient subject message_body [priority [format]]\n";
    std::cerr << "       \"priority\" has to be one of \"very_low\", \"low\", \"medium\", \"high\", or \"very_high\".\n";
    std::cerr << "       \"format\" has to be one of \"plain_text\" or \"html\".\n\n";
    std::exit(EXIT_FAILURE);
}


EmailSender::Priority StringToPriority(const std::string &priority_candidate) {
    if (priority_candidate == "very_low")
        return EmailSender::VERY_LOW;
    if (priority_candidate == "low")
        return EmailSender::LOW;
    if (priority_candidate == "medium")
        return EmailSender::MEDIUM;
    if (priority_candidate == "high")
        return EmailSender::HIGH;
    if (priority_candidate == "very_high")
        return EmailSender::VERY_HIGH;
    Error("\"" + priority_candidate + "\" is an unknown priority!");
}   


EmailSender::Format StringToFormat(const std::string &format_candidate) {
    if (format_candidate == "plain_text")
        return EmailSender::PLAIN_TEXT;
    else if (format_candidate == "html")
        return EmailSender::HTML;
    Error("\"" + format_candidate + "\" is an unknown format!");
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5 and argc != 6 and argc != 7)
        Usage();

    EmailSender::Priority priority;
    EmailSender::Format format(EmailSender::PLAIN_TEXT);
    if (argc == 5)
        priority = EmailSender::DO_NOT_SET_PRIORITY;
    else if (argc == 6)
        priority = StringToPriority(argv[5]);
    else { // Assume argc == 7.
        priority = StringToPriority(argv[5]);
        format = StringToFormat(argv[6]);
    }

    if (not EmailSender::SendEmail(argv[1], argv[2], argv[3], argv[4], priority, format))
        Error("failed to send your email!");
}
