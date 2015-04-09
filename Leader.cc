#include "Leader.h"
#include <cstdio>
#include "StringUtil.h"


const size_t Leader::LEADER_LENGTH(24);


bool Leader::ParseLeader(const std::string &leader_string, Leader ** const leader, std::string * const err_msg) {
    if (err_msg != NULL)
	err_msg->clear();

    if (leader == NULL) {
	if (err_msg != NULL)
	    *err_msg = "\"leader\" argument to Leader::ParseLeader must point to something!";
	return false;
    }

    if (leader_string.size() != LEADER_LENGTH) {
	if (err_msg != NULL)
	    *err_msg = "Leader length must be " + std::to_string(LEADER_LENGTH) +
		", found " + std::to_string(leader_string.size()) + "!";
	return false;
    }

    unsigned record_length;
    if (std::sscanf(leader_string.substr(0, 5).data(), "%5u", &record_length) != 1) {
	if (err_msg != NULL)
	    *err_msg = "Can't parse record length!";
	return false;
    }

    unsigned base_address_of_data;
    if (std::sscanf(leader_string.substr(12, 5).data(), "%5u", &base_address_of_data) != 1) {
	if (err_msg != NULL)
	    *err_msg = "Can't parse base address of data!";
	return false;
    }

    //
    // Validity checks:
    //

    // Check indicator count:
    if (leader_string[10] != '2') {
	if (err_msg != NULL)
	    *err_msg = "Invalid indicator count!";
	return false;
    }
  
    // Check subfield code length:
    if (leader_string[11] != '2') {
	if (err_msg != NULL)
	    *err_msg = "Invalid subfield code length!";
	return false;
    }

    // Check entry map:
    if (leader_string.substr(20, 4) != "4500") {
	if (err_msg != NULL)
	    *err_msg = "Invalid entry map!";
	return false;
    }

    *leader = new Leader(leader_string, record_length, base_address_of_data);
    return true;
}


bool Leader::setRecordLength(const unsigned new_record_length, std::string * const err_msg) {
    if (err_msg != NULL)
	err_msg->clear();

    if (new_record_length > 99999) {
	*err_msg = "new record length (" + std::to_string(new_record_length)
                   + ") exceeds valid maximum (99999)!";
	return false;
    }

    record_length_ = new_record_length;
    raw_leader_ = StringUtil::PadLeading(std::to_string(record_length_), 5, '0') + raw_leader_.substr(5);
    return true;
}


void Leader::setBaseAddressOfData(const unsigned new_base_address_of_data) {
    base_address_of_data_ = new_base_address_of_data;
    raw_leader_ = raw_leader_.substr(0, 12) + StringUtil::PadLeading(std::to_string(base_address_of_data_), 5, '0')
                  + raw_leader_.substr(17);
}

