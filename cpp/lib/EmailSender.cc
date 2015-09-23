/** \file   EmailSender.cc
 *  \brief  Utility functions etc. related to the sending of email messages.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "EmailSender.h"
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "util.h"


namespace {


/** \brief Temorarily replace an environment variable's value. */
class ReplaceEnvVar {
    std::string variable_name_;
    std::string old_value_;
public:
    /** \brief Replace the value w/ "temp_value". */
    ReplaceEnvVar(const std::string &variable_name, const std::string &temp_value) ;

    /** \brief Restore the old value. */
    ~ReplaceEnvVar();
};


ReplaceEnvVar::ReplaceEnvVar(const std::string &variable_name, const std::string &temp_value) {
    const char * const old_value(::getenv(variable_name.c_str()));
    if (old_value != nullptr) {
	variable_name_ = variable_name;
	old_value_     = old_value;
    }

    if (unlikely(::setenv(variable_name.c_str(), temp_value.c_str(), /* overwrite = */ true) != 0))
	Error("setenv(3) failed in ReplaceEnvVar::ReplaceEnvVar! (errno: " + std::to_string(errno) + ")");
}
    

ReplaceEnvVar:: ~ReplaceEnvVar() {
    if (not variable_name_.empty()) {
	if (unlikely(::setenv(variable_name_.c_str(), old_value_.c_str(), /* overwrite = */ true) != 0))
	    Error("setenv(3) failed in ReplaceEnvVar::~ReplaceEnvVar! (errno: " + std::to_string(errno) + ")");
    }
}


} // unnamed namespace


namespace EmailSender {


bool SendEmail(const std::string &sender, const std::string &recipient, const std::string &subject,
	       const std::string &message_body)
{
    ReplaceEnvVar replace_env_var("PATH", "/bin:/usr/bin");
    const std::string MAILX_PATH(ExecUtil::Which("mailx"));
    if (unlikely(MAILX_PATH.empty()))
	Error("in EmailSender::SendEmail: can't find \"mailx\"!");

    FileUtil::AutoTempFile auto_temp_file;
    const std::string &stdin_replacement_for_mailx(auto_temp_file.getFilePath());
    if (not FileUtil::WriteString(stdin_replacement_for_mailx, message_body))
	Error("in EmailSender::SendEmail: can't write the message body into a temporary file!");

    return ExecUtil::Exec(MAILX_PATH, { "-a", "Reply-To: " + sender, "-s ", subject, recipient },
			  stdin_replacement_for_mailx) == 0;
}


} // namespace EmailSender
