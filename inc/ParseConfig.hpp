#ifndef PARSECONF_HPP
#define PARSECONF_HPP

#include "WebServ.hpp"
#include "ServerConfig.hpp"

class ParseConfig {
private:
    ServerConfig config;

	void swap(ParseConfig &other);
    std::ifstream openFile(char *file);
    void parseHeader(std::ifstream &fileStream);
    void parseConfigBlock(std::ifstream &fileStream);
    void trim(std::string &line);
    void parseDirective(std:ifstream &fileStream, std::string &line);
	std::string findValue(size_t pos, std::string line);
public:
    // Constructors, Destructor, and Operators
    ParseConfig();

    ParseConfig(const ParseConfig &copy);

    ParseConfig &operator=(ParseConfig copy);

    ~ParseConfig();

    ParseConfig(char *file);

    ServerConfig getConfig() const;

    class CouldNotOpenFile : public std::exception {
    public:
        const char *what() const throw();
    };

    class InvalidFormat : public std::exception {
    public:
        const char *what() const throw();
    };
};

#endif