#ifndef PARSECONF_HPP
#define PARSECONF_HPP

#include "WebServ.hpp"
#include "ServerConf.hpp"

class ParseConf
{
private:
	ServerConf	conf;
public:
	ParseConf();
	ParseConf(char *file);
	ParseConf(const ParseConf &copy);
	ParseConf	operator=(const ParseConf &copy);
	~ParseConf();
	ServerConf	getConf() const;
	std::string findValue(size_t pos, std::string line);
	class	CouldNotOpenFile : public std::exception {
	public:
		const char	*what() const throw();
	};
	class	InvalidFormat : public std::exception {
	public:
		const char	*what() const throw();
	};
};


#endif
