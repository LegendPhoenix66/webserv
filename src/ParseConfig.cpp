#include "../inc/ParseConfig.hpp"

ParseConfig::ParseConfig() {
}

ParseConfig::ParseConfig(const ParseConfig &copy)
		: configs(copy.configs) {
}

ParseConfig &ParseConfig::operator=(ParseConfig copy) {
	this->swap(copy);
	return *this;
}

ParseConfig::~ParseConfig() {
}

void ParseConfig::swap(ParseConfig &other) {
	std::swap(this->configs, other.configs);
}

ParseConfig::ParseConfig(char *file) {
	std::ifstream	fileStream;
	fileStream.open(file);
	if (!fileStream.is_open())
		throw CouldNotOpenFile();
	std::vector<std::string>	conf_vec;
	std::string					line;
	while (std::getline(fileStream, line))
		conf_vec.push_back(line);
	size_t i = 0;
	while (i < conf_vec.size()) {
		parseHeader(conf_vec, i);
		parseConfigBlock(conf_vec, i);
	}
	fileStream.close();
}

void ParseConfig::parseHeader(std::vector<std::string> &conf_vec, size_t &i)
{
	for (; i < conf_vec.size(); i++) {
		trim(conf_vec[i]);
		if (conf_vec[i].empty() || conf_vec[i][0] == '#')
			continue;
		break;
	}

	if (i >= conf_vec.size())
		throw InvalidFormat("Config File: Empty file.");

	std::istringstream ss(conf_vec[i]);
	std::string keyword;
	std::string	temp;
	ss >> keyword;
	if (keyword != "server")
		throw InvalidFormat("Config File: Invalid header.");

	if (conf_vec[i].find('{') == std::string::npos) {
		i++;
		if (i >= conf_vec.size())
			throw InvalidFormat("Config File: Missing server block.");
		trim(conf_vec[i]);
		if (conf_vec[i] != "{")
			throw InvalidFormat("Config File: Missing '{' at start of server block.");
	}
	i++;
}

void ParseConfig::parseConfigBlock(std::vector<std::string> &conf_vec, size_t &i)
{
	ServerConfig	config;
	bool			end = false;

	for (; i < conf_vec.size(); i++) {
		trim(conf_vec[i]);
		if (conf_vec[i].empty() || conf_vec[i][0] == '#')
			continue;
		if (conf_vec[i][0] == '}') {
			this->configs.push_back(config);
			end = true;
			break;
		}
		parseDirective(conf_vec, i, config);
	}
	i++;

	if (!end)
		throw InvalidFormat("Config File: Missing '}' at end of server block.");
}

void ParseConfig::trim(std::string &line) {
	line.erase(0, line.find_first_not_of(" \t\n\r"));
	line.erase(line.find_last_not_of(" \t\n\r") + 1);
}

void ParseConfig::parseDirective(std::vector<std::string> &conf_vec, size_t &i, ServerConfig &config)
{
	std::string	first_word = conf_vec[i].substr(0, conf_vec[i].find_first_of(" \t"));

	if (first_word == "location") {
		handleLocation(conf_vec, i, config);
		return;
	}

	if (conf_vec[i].empty() || conf_vec[i][conf_vec[i].size() - 1] != ';')
		throw InvalidFormat("Config File: Missing ';' at end of line.");
	conf_vec[i].erase(conf_vec[i].size() - 1);

	std::istringstream	iss(conf_vec[i]);
	std::string			var;
	iss >> var;

	if (var == "listen")
		handleListen(iss, config);
	else if (var == "server_name" || var == "root" || var == "host")
		handleSimpleDirective(var, iss, config);
	else if (var == "index")
		handleIndex(iss, config);
	else if (var == "error_page")
		handleErrorPage(iss, config);
	else if (var == "client_max_body_size")
		handleClientSize(iss, config);
	else
		throw InvalidFormat("Config File: Unknown directive in server block.");
}

void	ParseConfig::handleClientSize(std::istringstream &iss, ServerConfig &config)
{
	std::string value_str;
	if (!(iss >> value_str))
		throw InvalidFormat("Config File: Missing value for client_max_body_size.");

	char	*endptr;
	long long value = std::strtoll(value_str.c_str(), &endptr, 10);

	if (endptr == value_str.c_str() || value < 0)
		throw InvalidFormat("Config File: Invalid value for client_max_body_size.");

	size_t	multiplier = 1;
	if (*endptr != '\0') {
		if ((*endptr == 'k' || *endptr == 'K') && *(endptr + 1) == '\0')
			multiplier *= 1024;
		else if ((*endptr == 'm' || *endptr == 'M') && *(endptr + 1) == '\0')
			multiplier *= 1024 * 1024;
		else
			throw InvalidFormat("Config File: Invalid value for client_max_body_size.");
	}

	config.setClientMaxBodySize(static_cast<size_t>(value) * multiplier);
}

void	ParseConfig::handleLocation(std::vector<std::string> &conf_vec, size_t &i, ServerConfig &config)
{
	Location	loc(conf_vec, i);
	config.addLocationBack(loc);
}

void	ParseConfig::handleListen(std::istringstream &iss, ServerConfig &config)
{
	int value;
	if (!(iss >> value) || value < 0 || value > 65535)
		throw InvalidFormat("Config File: Invalid port number.");
	config.setPort(static_cast<unsigned short>(value));
}

void	ParseConfig::handleSimpleDirective(const std::string &var, std::istringstream &iss, ServerConfig &config)
{
	std::string value;
	if (!(iss >> value))
		throw InvalidFormat("Config File: Missing value for " + var + ".");
	if (var == "server_name") config.setServerName(value);
	else if (var == "root") config.setRoot(value);
	else config.setHost(value);
}

void	ParseConfig::handleIndex(std::istringstream &iss, ServerConfig &config)
{
	std::string value;
	while (iss >> value)
		config.addIndexBack(value);
}

void	ParseConfig::handleErrorPage(std::istringstream &iss, ServerConfig &config)
{
	std::vector <std::string> args;
	std::string arg;
	while (iss >> arg)
		args.push_back(arg);

	if (args.size() < 2)
		throw InvalidFormat("Config File: Invalid error_page directive.");

	std::string url = args.back();
	args.pop_back();
	for (size_t i = 0; i < args.size(); ++i) {
		char *endptr;
		long value = std::strtol(args[i].c_str(), &endptr, 10);
		if (*endptr != '\0' || value < 0 || value > 65535)
			throw InvalidFormat("Config File: Invalid error code in error_page directive.");
		config.addErrorPageBack(static_cast<unsigned short>(value), url);
	}
}

std::vector<ServerConfig>	ParseConfig::getConfigs() const {
	return (this->configs);
}

const char	*ParseConfig::CouldNotOpenFile::what() const throw() {
	return "Could not open configuration file.";
}

ParseConfig::InvalidFormat::InvalidFormat(std::string message) : message(message)
{}

ParseConfig::InvalidFormat::~InvalidFormat() throw()
{}

const char	*ParseConfig::InvalidFormat::what() const throw() {
	return this->message.c_str();
}
