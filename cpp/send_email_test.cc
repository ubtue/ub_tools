#include <iostream>
#include <cstdlib>
#include "EmailSender.h"
#include "StringUtil.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " [--sender=sender] [-reply-to=reply_to] --recipient=recipient "
              << "--subject=subject--message-body=message_body [--priority=priority] [--format=format]\n"
              << "       \"priority\" has to be one of \"very_low\", \"low\", \"medium\", \"high\", or\n"
              << "       \"very_high\".  \"format\" has to be one of \"plain_text\" or \"html\"  At least one\n"
              << "       of \"sender\" or \"reply-to\" has to be specified.\n\n";
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


bool ExtractArg(const char * const argument, const std::string &arg_name, std::string * const arg_value) {
    if (StringUtil::StartsWith(argument, "--" + arg_name + "=")) {
        *arg_value = argument + arg_name.length() + 3 /* two dashes and one equal sign */;
        if (arg_value->empty())
            Error(arg_name + " is missing!");

        return true;
    }

    return false;
}


void ParseCommandLine(char **argv, std::string * const sender, std::string * const reply_to,
                      std::string * const recipient, std::string * const subject, std::string * const message_body,
                      std::string * const priority, std::string * const format)
{
    while (*argv != nullptr) {
        if (ExtractArg(*argv, "sender", sender) or ExtractArg(*argv, "reply-to", reply_to)
            or ExtractArg(*argv, "recipient", recipient) or ExtractArg(*argv, "subject", subject)
            or ExtractArg(*argv, "message-body", message_body) or ExtractArg(*argv, "priority", priority)
            or ExtractArg(*argv, "format", format))
            ++argv;
        else
            Error("unknown argument: " + std::string(*argv));
    }

    if (sender->empty() and reply_to->empty())
        Error("you must specify --sender and/or --reply-to!");
    if (recipient->empty())
        Error("you must specify a recipient!");
    if (subject->empty())
        Error("you must specify a subject!");
    if (message_body->empty())
        Error("you must specify a message-body!");
}


int main(int /*argc*/, char *argv[]) {
    ::progname = argv[0];

    EmailSender::Priority priority(EmailSender::DO_NOT_SET_PRIORITY);
    EmailSender::Format format(EmailSender::PLAIN_TEXT);

    std::string sender, reply_to, recipient, subject, message_body, priority_as_string, format_as_string;
    ParseCommandLine(++argv, &sender, &reply_to, &recipient, &subject, &message_body, &priority_as_string,
                     &format_as_string);

    if (not priority_as_string.empty())
        priority = StringToPriority(priority_as_string);
    if (not format_as_string.empty())
        format = StringToFormat(format_as_string);

    if (not EmailSender::SendEmail(sender, recipient, subject, message_body, priority, format, reply_to))
        Error("failed to send your email!");
}
