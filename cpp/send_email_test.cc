#include <iostream>
#include <cstdlib>
#include "EmailSender.h"
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " sender recipient subject message_body [priority]\n";
    std::cerr << "       \"priority\" has to be one of \"very_low\", \"low\", \"medium\", \"high\", or \"very_high\".\n";
    std::exit(EXIT_FAILURE);
}


EmailSender::Priority StringToPriority(const std::string &priority) {
    if (priority == "very_low")
	return EmailSender::VERY_LOW;
    if (priority == "low")
	return EmailSender::LOW;
    if (priority == "medium")
	return EmailSender::MEDIUM;
    if (priority == "high")
	return EmailSender::HIGH;
    if (priority == "very_high")
	return EmailSender::VERY_HIGH;
    Error("\"" + priority + "\" is an unknown priority!");
}   



int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5 and argc != 6)
	Usage();

    EmailSender::Priority priority;
    if (argc == 5)
	priority = EmailSender::DO_NOT_SET_PRIORITY;
    else
	priority = StringToPriority(argv[5]);

    if (not EmailSender::SendEmail(argv[1], argv[2], argv[3], argv[4], priority))
	Error("failed to send your email!");
}
