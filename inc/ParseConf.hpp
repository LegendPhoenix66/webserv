#ifndef PARSECONF_HPP
#define PARSECONF_HPP

#include "WebServ.hpp"
#include "ServerConf.hpp"

class ParseConf {
private:
    ServerConf conf;

	void swap(ParseConf &other);
	std::string findValue(size_t pos, std::string line);
public:
    // Constructors, Destructor, and Operators
    ParseConf();

    ParseConf(char *file);

    ParseConf(const ParseConf &copy);

    ParseConf &operator=(ParseConf copy);

    ~ParseConf();

    // Public Interface
    ServerConf getConf() const;

    // Exception Classes
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