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

ParseConfig::ParseConfig(std::string file) {
	if (isDirectory(file))
		throw IsDirectoryError();
	std::ifstream	fileStream;
	fileStream.open(file.c_str());
	if (!fileStream.is_open())
		throw CouldNotOpenFile();
	std::vector<std::string>	conf_vec;
	std::string					line;
	while (std::getline(fileStream, line))
		conf_vec.push_back(line);
	size_t i = 0;
	checkBrackets(conf_vec);
	while (i < conf_vec.size()) {
		parseHeader(conf_vec, i);
		parseConfigBlock(conf_vec, i);
	}

	fileStream.close();
}

void	ParseConfig::checkBrackets(const std::vector<std::string> conf_vec) {
	int	open_count = 0;
	for (size_t i = 0; i < conf_vec.size(); i++) {
		open_count += std::count(conf_vec[i].begin(), conf_vec[i].end(), '{');
		open_count -= std::count(conf_vec[i].begin(), conf_vec[i].end(), '}');
	}
	if (open_count != 0)
		throw InvalidFormat("Unclosed server block.");
}

void ParseConfig::parseHeader(std::vector<std::string> &conf_vec, size_t &i)
{
	while (i < conf_vec.size()) {
		conf_vec[i] = trim(conf_vec[i]);
		if (!conf_vec[i].empty() && conf_vec[i][0] != '#')
			break;
		i++;
	}
	if (i >= conf_vec.size())
		throw InvalidFormat("Empty file.");

	std::istringstream ss(conf_vec[i]);
	std::string keyword;
	ss >> keyword;
	if (keyword != "server")
		throw InvalidFormat("Invalid header.");

	if (conf_vec[i].find('{') == std::string::npos) {
		i++;
		conf_vec[i] = trim(conf_vec[i]);
		if (i >= conf_vec.size() || conf_vec[i] != "{")
			throw InvalidFormat("Missing '{' at start of server block.");
	}
	i++;
}

void ParseConfig::parseConfigBlock(std::vector<std::string> &conf_vec, size_t &i)
{
	ServerConfig	config;

	for (; i < conf_vec.size(); i++) {
		conf_vec[i] = trim(conf_vec[i]);
		if (conf_vec[i].empty() || conf_vec[i][0] == '#')
			continue;
		if (conf_vec[i][0] == '}') {
			this->configs.push_back(config);
			break;
		}
		parseDirective(conf_vec, i, config);
	}
	i++;
}

void ParseConfig::parseDirective(std::vector<std::string> &conf_vec, size_t &i, ServerConfig &config)
{
	const std::string	first_word = conf_vec[i].substr(0, conf_vec[i].find_first_of(" \t"));

	if (first_word == "location") {
		handleLocation(conf_vec, i, config);
		return;
	}

	const size_t		semicolon_pos = findLineEnd(conf_vec[i]);
	std::istringstream	iss(conf_vec[i].substr(0, semicolon_pos));
	std::string			var;
	iss >> var;
	std::string			dir_args = trim(conf_vec[i].substr(var.size(), semicolon_pos - var.size()));

	if (var == "listen")
		handleListen(iss, config);
	else if (var == "root")
		handleRoot(var, dir_args, config);
	else if (var == "host")
		handleHost(iss, config);
	else if (var == "index")
		handleIndex(var, dir_args, config);
	else if (var == "error_page")
		handleErrorPage(var, dir_args, config);
	else if (var == "client_max_body_size")
		handleClientSize(iss, config);
	else if (!var.empty())
		throw InvalidFormat("Unknown directive in server block.");
}

void	ParseConfig::handleClientSize(std::istringstream &iss, ServerConfig &config)
{
	if (config.getClientMaxBodySize() != 0)
		throw InvalidFormat("Duplicate client_max_body_size directive.");

	std::string value_str;
	if (!(iss >> value_str))
		throw InvalidFormat("Missing value for client_max_body_size.");

	char	*endptr;
	long long value = std::strtoll(value_str.c_str(), &endptr, 10);

	if (endptr == value_str.c_str() || value < 0)
		throw InvalidFormat("Invalid value for client_max_body_size.");

	size_t	multiplier = 1;
	if (*endptr != '\0') {
		if ((*endptr == 'k' || *endptr == 'K') && *(endptr + 1) == '\0')
			multiplier *= 1024;
		else if ((*endptr == 'm' || *endptr == 'M') && *(endptr + 1) == '\0')
			multiplier *= 1024 * 1024;
		else
			throw InvalidFormat("Invalid value for client_max_body_size.");
	}

	config.setClientMaxBodySize(static_cast<size_t>(value) * multiplier);
	if (iss >> value_str)
		throw InvalidFormat("client_max_body_size directive requires only one argument.");
}

void	ParseConfig::handleLocation(std::vector<std::string> &conf_vec, size_t &i, ServerConfig &config)
{
	Location	loc(conf_vec, i);
	config.addLocationBack(loc);
}

void	ParseConfig::handleHost(std::istringstream &iss, ServerConfig &config)
{
	if (config.getHost())
		throw InvalidFormat("Duplicate host directive.");
	std::string value;
	if (!(iss >> value))
		throw InvalidFormat("Missing value for host.");

	if (value == "localhost")
		value = "127.0.0.1";

	struct addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *res = NULL;
	int	ret = getaddrinfo(value.c_str(), NULL, &hints, &res);
	if (ret == 0 && res != NULL) {
		struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
		config.setHost(addr->sin_addr.s_addr);
	}
	else
		throw InvalidFormat("Invalid host address.");
	freeaddrinfo(res);
	if (iss >> value)
		throw InvalidFormat("Host directive requires only one argument.");
}

void	ParseConfig::handleListen(std::istringstream &iss, ServerConfig &config)
{
	if (config.getPort() != 0)
		throw InvalidFormat("Duplicate listen directive.");

	int value;
	if (!(iss >> value) || value < 0 || value > 65535)
		throw InvalidFormat("Invalid port number.");
	config.setPort(static_cast<unsigned short>(value));
	if (iss >> value)
		throw InvalidFormat("Listen directive requires only one argument.");
}

void	ParseConfig::handleRoot(const std::string var, const std::string line, ServerConfig &config)
{
	if (!config.getRoot().empty())
		throw InvalidFormat("Duplicate root directive.");
	std::string	path = extractSinglePath(var, line);
	config.setRoot(path);
}

void	ParseConfig::handleIndex(const std::string var, const std::string line, ServerConfig &config)
{
	if (!config.getIndex().empty())
		throw InvalidFormat("Duplicate index directive.");

	config.setIndex(extractQuotedArgs(var, line));
}

void ParseConfig::handleErrorPage(std::string var, std::string line, ServerConfig &config)
{
	std::vector<std::string>	args = extractQuotedArgs(var, line);

	std::string	status_file = args.back();
	args.pop_back();

	for (size_t i = 0; i < args.size(); i++) {
		if (!isCode(args[i]) || !checkValidCode(std::atoi(args[i].c_str())))
			throw InvalidFormat("Invalid status code in error_page directive.");
		config.addErrorPageBack(std::atoi(args[i].c_str()), status_file);
	}
}

std::vector<ServerConfig>	ParseConfig::getConfigs() const {
	return (this->configs);
}

const char	*ParseConfig::CouldNotOpenFile::what() const throw() {
	return "Could not open configuration file.";
}

const char	*ParseConfig::IsDirectoryError::what() const throw() {
	return "Input file is a directory.";
}
