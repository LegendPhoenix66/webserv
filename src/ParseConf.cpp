#include "../inc/ParseConf.hpp"

ParseConf::ParseConf()
{

}

ParseConf::ParseConf(char *file)
{
	std::ifstream	conffile(file);
	if (!conffile.is_open())
		throw CouldNotOpenFile();

	std::string			line;
	std::getline(conffile, line);
	size_t	first_char = line.find_first_not_of(" \t");
	while (first_char == std::string::npos) {
		std::getline(conffile, line);
		first_char = line.find_first_not_of(" \t");
	}
	if (line.substr(0, 6) != "server")
		throw InvalidFormat();
	if (line.find('{', 6) == std::string::npos)
		std::getline(conffile, line);
	if (line.find('{') == std::string::npos)
		throw InvalidFormat();

	bool	brace = false;
	while (std::getline(conffile, line) && line.find('}') == std::string::npos && !brace) {
		size_t	start = line.find_first_not_of(" \t");
		if (start == std::string::npos)
			continue ;
		if (line.substr(start, 6) == "listen")
			this->conf.setPort(std::atoi(findValue(start + 6, line).c_str()));
		else if (line.substr(start, 11) == "server_name")
			this->conf.setServerName(findValue(start + 11, line));
		else if (line.substr(start, 4) == "root")
			this->conf.setRoot(findValue(start + 4, line));
		else if (line.substr(start, 4) == "host")
			this->conf.setHost(findValue(start + 4, line));
	}
	conffile.close();
}

ParseConf::ParseConf(const ParseConf &copy)
{
	*this = copy;
}

ParseConf	ParseConf::operator=(const ParseConf &copy)
{
	if (this != &copy) {
		this->conf = copy.conf;
	}
	return *this;
}

ParseConf::~ParseConf()
{
}

std::string	ParseConf::findValue(size_t pos, std::string line)
{
	size_t	start = line.find_first_not_of(" \t", pos);
	if (start == std::string::npos)
		throw InvalidFormat();

	size_t	end = line.find_first_of(';', start);
	if (end == std::string::npos)
		throw InvalidFormat();

	return (line.substr(start, end - start));
}

ServerConf	ParseConf::getConf() const
{
	return (this->conf);
}

const char	*ParseConf::CouldNotOpenFile::what() const throw()
{
	return "could not open file.";
}

const char	*ParseConf::InvalidFormat::what() const throw()
{
	return "invalid format.";
}
