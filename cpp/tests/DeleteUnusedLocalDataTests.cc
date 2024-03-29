/** \brief Test cases for delete_unused_local_data
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <vector>
#include "ExecUtil.h"
#include "UnitTest.h"


TEST(DeleteUnusedBlocks) {
    int exit_code(ExecUtil::Exec("/usr/local/bin/delete_unused_local_data",
                                 { "/usr/local/ub_tools/cpp/tests/data/LOK_withoutUsedBlock.mrc", "/tmp/output.mrc" }));
    CHECK_EQ(exit_code, 0);

    exit_code = ExecUtil::Exec("/usr/bin/diff", { "/usr/local/ub_tools/cpp/tests/data/default.mrc", "/tmp/output.mrc" });
    CHECK_EQ(exit_code, 0);
}


TEST(DeleteUnusedBlockAndLeaveOneBlock) {
    int exit_code(ExecUtil::Exec("/usr/local/bin/delete_unused_local_data",
                                 { "/usr/local/ub_tools/cpp/tests/data/LOK_withUsedBlock.mrc", "/tmp/output.mrc" }));
    CHECK_EQ(exit_code, 0);

    exit_code = ExecUtil::Exec("/usr/bin/diff", { "/usr/local/ub_tools/cpp/tests/data/LOK_withUsedBlock.expected.mrc", "/tmp/output.mrc" });
    CHECK_EQ(exit_code, 0);
}


TEST(DeleteNoBlock) {
    int exit_code(
        ExecUtil::Exec("/usr/local/bin/delete_unused_local_data", { "/usr/local/ub_tools/cpp/tests/data/default.mrc", "/tmp/output.mrc" }));
    CHECK_EQ(exit_code, 0);

    exit_code = ExecUtil::Exec("/usr/bin/diff", { "/usr/local/ub_tools/cpp/tests/data/default.mrc", "/tmp/output.mrc" });
    CHECK_EQ(exit_code, 0);
}


TEST_MAIN(MarcRecord)
