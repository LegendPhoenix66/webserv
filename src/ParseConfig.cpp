#include "../inc/ParseConfig.hpp"

ParseConfig::ParseConfig() {
}

ParseConfig::ParseConfig(char *file) {
    std::ifstream conffile(file);
    if (!conffile.is_open())
        throw CouldNotOpenFile();

    std::string line;
    std::getline(conffile, line);

    std::istringstream ss(line);
    std::string keyword;

    ss >> keyword;
    if (keyword != "server")
        throw InvalidFormat();

    if (line.find('{') == std::string::npos) {
        if (!std::getline(conffile, line) || line.find('{') == std::string::npos) {
            throw InvalidFormat();
        }
    }

    while (std::getline(conffile, line)) {
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.empty() || line[0] == '#')
            continue;

        if (line[0] == '}')
            break;

        std::istringstream iss(line);
        std::string var;
        iss >> var;

        if (var == "location") {
            Location loc(conffile, line);
            conf.addLocationBack(loc);
            continue;
        }

        if (line[line.size() - 1] != ';')
            throw InvalidFormat();

        line.pop_back();

        if (var == "listen") {
            int value;
            if (!(iss >> value) || value < 0 || value > 65535)
                throw InvalidFormat();
            conf.setPort(static_cast<uint16_t>(value));
        } else if (var == "server_name" || var == "root" || var == "host") {
            std::string value;
            if (!(iss >> value))
                throw InvalidFormat();
            if (var == "server_name") conf.setServerName(value);
            else if (var == "root") conf.setRoot(value);
            else conf.setHost(value);
        } else if (var == "index") {
            std::string value;
            while (iss >> value)
                conf.addIndexBack(value);
        } else if (var == "error_page") {
            std::vector<std::string> args;
            std::string arg;
            while (iss >> arg)
                args.push_back(arg);

            if (args.size() < 2)
                throw InvalidFormat();

            std::string url = args.back();
            args.pop_back();
            for (size_t i = 0; i < args.size(); ++i) {
                for (size_t j = 0; j < args[i].size(); ++j) {
                    if (!std::isdigit(args[i][j]))
                        throw InvalidFormat();
                }
                conf.addErrorPageBack(std::atoi(args[i].c_str()), url);
            }
        } else
            throw InvalidFormat();
    }
}

ParseConfig::ParseConfig(const ParseConfig &copy)
: conf(copy.conf)
{
}

ParseConfig &ParseConfig::operator=(ParseConfig copy) {
	this->swap(copy);
    return *this;
}

ParseConfig::~ParseConfig() {
}

void ParseConfig::swap(ParseConfig &other) {
    std::swap(this->conf, other.conf);
}

std::string ParseConfig::findValue(size_t pos, std::string line) {
    size_t start = line.find_first_not_of(" \t", pos);
    if (start == std::string::npos)
        throw InvalidFormat();

    size_t end = line.find_first_of(';', start);
    if (end == std::string::npos)
        throw InvalidFormat();

    return (line.substr(start, end - start));
}

ServerConfig ParseConfig::getConfig() const {
    return (this->conf);
}

const char *ParseConfig::CouldNotOpenFile::what() const throw() {
    return "could not open file.";
}

const char *ParseConfig::InvalidFormat::what() const throw() {
    return "invalid format.";
}
