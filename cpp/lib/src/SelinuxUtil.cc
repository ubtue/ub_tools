/** \file   SelinuxUtil.cc
 *  \brief  Various utility functions related to SElinux
 *  \author Mario Trojan (mario.trojan@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include "SelinuxUtil.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"


namespace SelinuxUtil {


void AddFileContext(const std::string &context, const std::string &file_spec) {
    AssertEnabled(std::string(__func__));
    ExecUtil::Exec(ExecUtil::Which("semanage"), { "fcontext", "-a", "-t", context, file_spec });
}


void AddFileContextIfMissing(const std::string &path, const std::string &context, const std::string &file_spec) {
    if (not HasFileContext(path, context)) {
        std::cout << "missing, adding" << '\n';
        AddFileContext(context, file_spec);
        ApplyChanges(path);
    }

    if (not HasFileContext(path, context)) {
        throw new std::runtime_error("in " + std::string(__func__) +": "
                                     + "could not set context " + context + " for " + path + " using " + file_spec);
    }
}


void ApplyChanges(const std::string &path) {
    AssertEnabled(std::string(__func__));
    ExecUtil::Exec(ExecUtil::Which("restorecon"), { "-R", "-v", path });
}


void AssertEnabled(const std::string &caller) {
    if (GetMode() == DISABLED) {
        throw new std::runtime_error("in " + caller +": SElinux is disabled!");
    }
}


void AssertFileHasContext(const std::string &path, const std::string &context) {
    if (not HasFileContext(path, context)) {
        throw new std::runtime_error("in " + std::string(__func__) +": "
                                     + "file " + " doesn't have context " + context);
    }
}


std::vector<std::string> GetFileContexts(const std::string &path) {
    AssertEnabled(std::string(__func__));
    const std::string tempfile = "/tmp/filecontext";
    if (ExecUtil::Exec(ExecUtil::Which("ls"), { "-ldZ", path }, "", tempfile) != 0)
        throw new std::runtime_error("in " + std::string(__func__) +": "
                                     + "could not read file permission: " + path);
    else {
        std::string ls;
        std::cout << ls << '\n';
        FileUtil::ReadString(tempfile, &ls);
        StringUtil::TrimWhite(&ls);

        static const std::string FILE_CONTEXT_EXTRACTION_REGEX("^[^ ]+ [^ ]+ [^ ]+ ([^ ]+) .+$");
        static RegexMatcher *matcher;
        if (matcher == nullptr) {
            std::string err_msg;
            matcher = RegexMatcher::RegexMatcherFactory(FILE_CONTEXT_EXTRACTION_REGEX, &err_msg);
        }

        if (unlikely(not matcher->matched(ls)))
            throw new std::runtime_error("in " + std::string(__func__) +": \"" + ls + "\" failed to match the regex \""
                  + FILE_CONTEXT_EXTRACTION_REGEX + "\"!");

        std::vector<std::string> file_contexts;
        StringUtil::Split((*matcher)[1], ':', &file_contexts);
        return file_contexts;
    }
}


Mode GetMode() {
    const std::string getenforce_binary(ExecUtil::Which("getenforce"));
    if (not getenforce_binary.empty()) {
        const std::string tempfile("/tmp/getenforce");
        if (ExecUtil::Exec(getenforce_binary, {}, "", tempfile) == 0) {
            std::string getenforce;
            FileUtil::ReadString(tempfile, &getenforce);
            StringUtil::TrimWhite(&getenforce);

            if (getenforce == "Enforcing")
                return ENFORCING;
            else if (getenforce == "Permissive")
                return PERMISSIVE;
            else if (getenforce == "Disabled")
                return DISABLED;

            std::cout << "i cry" << '\n';
        }
    }

    // return DISABLED on error or if not installed
    return DISABLED;
}


bool HasFileContext(const std::string &path, const std::string &context) {
    std::vector<std::string> file_contexts = GetFileContexts(path);
    return (std::find(file_contexts.begin(), file_contexts.end(), context) != file_contexts.end());
}


}
