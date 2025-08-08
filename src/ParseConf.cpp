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

	std::istringstream	ss(line);
	std::string			keyword;

	ss >> keyword;
	if (keyword != "server")
		throw InvalidFormat();

	if (line.find('{') == std::string::npos) {
		if (!std::getline(conffile, line) || line.find('{') == std::string::npos) {
			throw InvalidFormat();
		}
	}

	while (std::getline(conffile, line)) {
		line.erase(0, line.find_first_not_of(" \t\n"));
		line.erase(line.find_last_not_of(" \t\n") + 1);

		if (line.empty() || line[0] == '#')
			continue;

		if (line[0] == '}')
			break;

		std::string	var;
		std::string	first_word = line.substr(0, line.find_first_of(" \t"));

		if (first_word == "location") {
			Location	loc(conffile, line);
			this->conf.addLocationBack(loc);
			continue;
		}

		if (line[line.size() - 1] != ';')
			throw InvalidFormat();
		line.erase(line.size() - 1);

		std::istringstream	iss(line);
		iss >> var;

		if (var == "listen") {
			uint16_t	value;
			if (!(iss >> value))
				throw InvalidFormat();
			this->conf.setPort(value);
		}
		else if (var == "server_name") {
			std::string	value;
			if (!(iss >> value))
				throw InvalidFormat();
			this->conf.setServerName(value);
		}
		else if (var == "root") {
			std::string	value;
			if (!(iss >> value))
				throw InvalidFormat();
			this->conf.setRoot(value);
		}
		else if (var == "host") {
			std::string	value;
			if (!(iss >> value))
				throw InvalidFormat();
			this->conf.setHost(value);
		}
		else if (var == "index") {
			std::string	value;
			while (iss >> value)
				this->conf.addIndexBack(value);
		}
		else if (var == "error_page") {
			std::vector<std::string>	args;
			std::string					arg;
			while (iss >> arg)
				args.push_back(arg);

			if (args.size() < 2)
				throw InvalidFormat();

			std::string	url = args.back();
			args.pop_back();
			for (size_t i = 0; i < args.size(); i++) {
				for (size_t j = 0; j < args[i].size(); j++) {
					if (!std::isdigit(args[i][j]))
						throw InvalidFormat();
				}
				this->conf.addErrorPageBack(std::atoi(args[i].c_str()), url);
			}
		}
		else
			throw InvalidFormat();
	}
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
