/**
 * @file ArgumentParser.cpp
 * @brief Implementation of argument parsing
 */

#include "multipack/core/ArgumentParser.h"
#include "multipack/config/ConfigDefaults.h"
#include <QDebug>

namespace multipack {
namespace core {

Arguments ArgumentParser::parse(int argc, char** argv)
{
    Arguments args;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--verbose" || arg == "-v") args.verbose = true;
        else if (arg == "--no-virtual-keyboard") args.noVirtualKeyboard = true;
        else if (arg == "--version" || arg == "-V") args.showVersion = true;
        else if (arg == "--license") args.showLicense = true;
        else if (arg == "--help" || arg == "-h") args.showHelp = true;
    }
    return args;
}

void ArgumentParser::printHelp()
{
    qInfo() << "Usage: multipack-parser [options]";
    qInfo() << "Options:";
    qInfo() << "  -v, --verbose            Enable debug logging";
    qInfo() << "  --no-virtual-keyboard    Disable virtual keyboard";
    qInfo() << "  -V, --version            Show version";
    qInfo() << "  --license                Show license";
    qInfo() << "  -h, --help               Show this help";
}

void ArgumentParser::printVersion()
{
    qInfo() << "MultipackParser version" << config::Defaults::VERSION;
}

void ArgumentParser::printLicense()
{
    qInfo() << "MultipackParser - Proprietary Software";
    qInfo() << "Copyright (c) 2024 Szaidel Cosmetic GmbH";
}

} // namespace core
} // namespace multipack
