/** \file   Main.h
 *  \brief  Default main entry point.
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <cstdlib>


int Main(int argc, char *argv[]);


int main(int argc, char *argv[]) __attribute__((weak));


// Registers a handler that gets executed before entering a program's Main entry point.
// Handlers with higher priority values are executed before those with lower priority values.
// Must be called before Main is executed.
void RegisterProgramPrologueHandler(const unsigned priority, const std::function<void()> &handler);

// Registers a handler that gets executed after a program's Main entry point has exitted.
// Handlers with higher priority values are executed before those with lower priority values.
// Must be called before Main is executed.
void RegisterProgramEpilogueHandler(const unsigned priority, const std::function<void()> &handler);
