/** \file    ExecUtil.h
 *  \brief   Declarations of MIME/media type utility functions.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef EXEC_UTIL_H
#define EXEC_UTIL_H


#include <string>
#include <vector>


/** \brief  Run a subcommand.
 *  \param  command             The path to the command that should be executed.
 *  \param  args                The arguments for the command, not including the command itself.
 *  \param  new_stdout          An optional replacement file path for the stdout.
 *  \param  timeout_in_seconds  If not zero, the subprocess will be killed if the timeout expires before
 *                              the process terminates.  SIGKILL will be used.
 *  \return The exit code of the subcommand or an error code if there was a failure along the way.
 */
int Exec(const std::string &command, const std::vector<std::string> &args = {}, const std::string &new_stdout = "",
	 unsigned timeout_in_seconds = 0);


#endif // ifndef EXEC_UTIL_H
