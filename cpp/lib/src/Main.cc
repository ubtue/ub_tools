/** \file   Main.cc
 *  \brief  Default main entry point.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Main.h"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <cerrno>
#include <cstdlib>
#include "StringUtil.h"
#include "util.h"


namespace {


struct MainHandler {
    enum HandlerType { PROLOGUE, EPILOGUE };

    HandlerType type_;
    unsigned priority_;
    std::function<void()> handler_;

public:
    MainHandler(const HandlerType type, const unsigned priority, const std::function<void()> &handler)
        : type_(type), priority_(priority), handler_(handler) { }
};


bool MainHandlerComparator(const MainHandler &a, const MainHandler &b) {
    return a.priority_ > b.priority_;
}


std::vector<MainHandler> *GetHandlers(const MainHandler::HandlerType type) {
    static std::vector<MainHandler> prologue_handlers;
    static std::vector<MainHandler> epilogue_handlers;

    switch (type) {
    case MainHandler::HandlerType::PROLOGUE:
        return &prologue_handlers;
    case MainHandler::HandlerType::EPILOGUE:
        return &epilogue_handlers;
    default:
        LOG_ERROR("unknown handler type " + std::to_string(type));
    }

    return nullptr;
}


void RunHandlers(const MainHandler::HandlerType type) {
    const auto handlers(GetHandlers(type));

    for (const auto &handler : *handlers)
        handler.handler_();
}


bool prologue_handlers_finalised(false);
bool epilogue_handlers_finalised(false);


} // unnamed namespace


void RegisterProgramPrologueHandler(const unsigned priority, const std::function<void()> &handler) {
    if (prologue_handlers_finalised)
        LOG_ERROR("prologue handlers have already been finalised!");

    GetHandlers(MainHandler::HandlerType::PROLOGUE)->emplace_back(MainHandler::HandlerType::PROLOGUE, priority, handler);
}


void RegisterProgramEpilogueHandler(const unsigned priority, const std::function<void()> &handler) {
    if (epilogue_handlers_finalised)
        LOG_ERROR("epilogue handlers have already been finalised!");

    GetHandlers(MainHandler::HandlerType::EPILOGUE)->emplace_back(MainHandler::HandlerType::EPILOGUE, priority, handler);
}


int Main(int argc, char *argv[]) __attribute__((weak));


int main(int argc, char *argv[]) __attribute__((weak));


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    Logger::LogLevel log_level(Logger::LL_INFO);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--min-log-level=")) {
        const std::string level(argv[1] + __builtin_strlen("--min-log-level="));
        if (level == "ERROR")
            log_level = Logger::LL_ERROR;
        else if (level == "WARNING")
            log_level = Logger::LL_WARNING;
        else if (level == "INFO")
            log_level = Logger::LL_INFO;
        else if (level == "DEBUG")
            log_level = Logger::LL_DEBUG;
        else
            LOG_ERROR("unknown log level \"" + level + "\"!");
        --argc, ++argv;
    }
    logger->setMinimumLogLevel(log_level);

    try {
        errno = 0;
        prologue_handlers_finalised = true;
        std::sort(GetHandlers(MainHandler::HandlerType::PROLOGUE)->begin(), GetHandlers(MainHandler::HandlerType::PROLOGUE)->end(),
                  MainHandlerComparator);
        RunHandlers(MainHandler::HandlerType::PROLOGUE);

        const auto ret_code(Main(argc, argv));

        epilogue_handlers_finalised = true;
        std::sort(GetHandlers(MainHandler::HandlerType::EPILOGUE)->begin(), GetHandlers(MainHandler::HandlerType::EPILOGUE)->end(),
                  MainHandlerComparator);
        RunHandlers(MainHandler::HandlerType::EPILOGUE);

        return ret_code;
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}
