/**
 * @file ArgumentParser.h
 * @brief Command line argument parsing
 */

#ifndef MULTIPACK_CORE_ARGUMENTPARSER_H
#define MULTIPACK_CORE_ARGUMENTPARSER_H

#include <QString>

namespace multipack {
namespace core {

struct Arguments {
    bool verbose = false;
    bool noVirtualKeyboard = false;
    bool showVersion = false;
    bool showLicense = false;
    bool showHelp = false;
};

class ArgumentParser
{
public:
    static Arguments parse(int argc, char** argv);
    static void printHelp();
    static void printVersion();
    static void printLicense();
};

} // namespace core
} // namespace multipack

#endif // MULTIPACK_CORE_ARGUMENTPARSER_H
