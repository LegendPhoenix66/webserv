#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <stdexcept>
#include <string>
#include "Config.hpp"

// Error kind to distinguish syntax vs validation
enum ConfigErrorKind {
    SYNTAX_ERROR = 1,
    VALIDATION_ERROR = 2,
    IO_ERROR = 3
};

class ConfigError : public std::runtime_error {
public:
    ConfigError(const std::string &file, int line, int col, const std::string &msg, ConfigErrorKind kind)
        : std::runtime_error(msg), _file(file), _line(line), _col(col), _kind(kind) {}

    // C++98/libstdc++: base ~runtime_error() is declared throw(); match it to avoid looser exception-spec.
    virtual ~ConfigError() throw() {}

    const std::string &file() const { return _file; }
    int line() const { return _line; }
    int col() const { return _col; }
    ConfigErrorKind kind() const { return _kind; }

private:
    std::string _file;
    int _line;
    int _col;
    ConfigErrorKind _kind;
};

class ConfigParser {
public:
    ConfigParser();
    Config parseFile(const std::string &path);
};

#endif // CONFIG_PARSER_HPP
