/** \file  CopyUsingRegex.cc
 *  \brief  Helper functions for copy file(s) using regex for file name
 *  \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 *  \copyright 2022 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <iostream>
#include <regex>
#include <string>
#include <vector>
#include <dirent.h>
#include "CopyUsingRegex.h"
#include "FileUtil.h"


void GetAllFiles(const char* srcPath, std::vector<char*>& fileList) {
    DIR* dir;
    struct dirent* diread;

    if ((dir = opendir(srcPath)) != nullptr) {
        while ((diread = readdir(dir)) != nullptr) {
            fileList.push_back(diread->d_name);
        }
        closedir(dir);
    } else {
        perror("opendir");
    }
}

void CopyUsingRegex::CopyFiles(const char* srcPath, const char* desPath, std::string fName) {
    std::vector<char*> fileList;
    std::string sourcePath(srcPath);
    std::string destPath(desPath);
    GetAllFiles(srcPath, fileList);

    for (auto f : fileList) {
        if (regex_search(f, std::regex(fName))) {
            sourcePath = srcPath;
            destPath = desPath;
            sourcePath = sourcePath.append(f);
            destPath = destPath.append(f);
            std::cout << "Match file: " << f << std::endl;
            std::cout << "Copying from: " << sourcePath << " to: " << destPath << std::endl;
            FileUtil::CopyOrDie(sourcePath, destPath);
        }
    }
}
