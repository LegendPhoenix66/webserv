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
	while (fileStream.peek() != EOF) {
		parseHeader(fileStream);
		parseConfigBlock(fileStream);
	}
	fileStream.close();
}

void ParseConfig::parseHeader(std::ifstream &fileStream) {
	std::string	line;
	while (std::getline(fileStream, line)) {
		trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		break;
	}
	std::istringstream ss(line);
	std::string keyword;
	ss >> keyword;
	if (keyword != "server")
		throw InvalidFormat();
	if (line.find('{') == std::string::npos) {
		if (!std::getline(fileStream, line) || line.find('{') == std::string::npos)
			throw InvalidFormat();
	}
}

void ParseConfig::parseConfigBlock(std::ifstream &fileStream) {
	std::string		line;
	ServerConfig	config;
	while (std::getline(fileStream, line)) {
		trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		if (line[0] == '}') {
			this->configs.push_back(config);
			break;
		}
		parseDirective(fileStream, line, config);
	}
}

void ParseConfig::trim(std::string &line) {
	line.erase(0, line.find_first_not_of(" \t\n\r"));
	line.erase(line.find_last_not_of(" \t\n\r") + 1);
}

void ParseConfig::parseDirective(std::ifstream &fileStream, std::string &line, ServerConfig &config)
{
	std::string	first_word = line.substr(0, line.find_first_of(" \t"));

	if (first_word == "location") {
		handleLocation(fileStream, line, config);
		return;
	}

	if (line.empty() || line[line.size() - 1] != ';')
		throw InvalidFormat();
	line.erase(line.size() - 1);

	std::istringstream	iss(line);
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
		throw InvalidFormat();
}

void	ParseConfig::handleClientSize(std::istringstream &iss, ServerConfig &config)
{
	int value;
	if (!(iss >> value) || value < 0)
		throw InvalidFormat();
	config.setClientMaxBodySize(static_cast<size_t>(value));
}

void	ParseConfig::handleLocation(std::ifstream &fileStream, std::string &line, ServerConfig &config)
{
	Location loc(fileStream, line);
	config.addLocationBack(loc);
}

void	ParseConfig::handleListen(std::istringstream &iss, ServerConfig &config)
{
	int value;
	if (!(iss >> value) || value < 0 || value > 65535)
		throw InvalidFormat();
	config.setPort(static_cast<unsigned short>(value));
}

void	ParseConfig::handleSimpleDirective(const std::string &var, std::istringstream &iss, ServerConfig &config)
{
	std::string value;
	if (!(iss >> value))
		throw InvalidFormat();
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
		throw InvalidFormat();

	std::string url = args.back();
	args.pop_back();
	for (size_t i = 0; i < args.size(); ++i) {
		char *endptr;
		long value = std::strtol(args[i].c_str(), &endptr, 10);
		if (*endptr != '\0' || value < 0 || value > 65535)
			throw InvalidFormat();
		config.addErrorPageBack(static_cast<unsigned short>(value), url);
	}
}

std::string	ParseConfig::findValue(size_t pos, std::string line) {
	size_t start = line.find_first_not_of(" \t", pos);
	if (start == std::string::npos)
		throw InvalidFormat();

	size_t end = line.find_first_of(';', start);
	if (end == std::string::npos)
		throw InvalidFormat();

	return (line.substr(start, end - start));
}

std::vector<ServerConfig>	ParseConfig::getConfigs() const {
	return (this->configs);
}

const char	*ParseConfig::CouldNotOpenFile::what() const throw() {
	return "could not open file.";
}

const char	*ParseConfig::InvalidFormat::what() const throw() {
	return "invalid format.";
}
