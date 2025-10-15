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
		throw InvalidFormat("Config File: Unclosed server block.");
}

void ParseConfig::parseHeader(std::vector<std::string> &conf_vec, size_t &i)
{
	while (i < conf_vec.size()) {
		trim(conf_vec[i]);
		if (!conf_vec[i].empty() && conf_vec[i][0] != '#')
			break;
		i++;
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
		if (i >= conf_vec.size() || trim(conf_vec[i]) != "{")
			throw InvalidFormat("Config File: Missing '{' at start of server block.");
	}
	i++;
}

void ParseConfig::parseConfigBlock(std::vector<std::string> &conf_vec, size_t &i)
{
	ServerConfig	config;

	for (; i < conf_vec.size(); i++) {
		trim(conf_vec[i]);
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

std::string	ParseConfig::trim(std::string &line) {
	line.erase(0, line.find_first_not_of(" \t\n\r"));
	line.erase(line.find_last_not_of(" \t\n\r") + 1);
	return line;
}

void ParseConfig::parseDirective(std::vector<std::string> &conf_vec, size_t &i, ServerConfig &config)
{
	const std::string	first_word = conf_vec[i].substr(0, conf_vec[i].find_first_of(" \t"));

	if (first_word == "location") {
		handleLocation(conf_vec, i, config);
		return;
	}

	const size_t	semicolon_pos = conf_vec[i].find(';');
	if (conf_vec[i].empty() || semicolon_pos == std::string::npos)
		throw InvalidFormat("Config File: Missing ';' at end of line.");

	const size_t	comment_pos = conf_vec[i].find('#', semicolon_pos);
	std::string trailing;
	if (comment_pos != std::string::npos)
		trailing = conf_vec[i].substr(semicolon_pos + 1, comment_pos - semicolon_pos - 1);
	else
		trailing = conf_vec[i].substr(semicolon_pos + 1);
	for (size_t j = 0; j < trailing.size(); j++) {
		if (!isspace(trailing[j]))
			throw InvalidFormat("Config File: Unexpected characters after ';'.");
	}

	conf_vec[i] = conf_vec[i].substr(0, semicolon_pos);

	std::istringstream	iss(conf_vec[i]);
	std::string			var;
	iss >> var;
	std::string			dir_args = conf_vec[i].substr(var.size(), semicolon_pos - var.size());
	trim(dir_args);

	if (var == "listen")
		handleListen(iss, config);
	else if (var == "server_name" || var == "root")
		handleSimpleDirective(var, iss, config);
	else if (var == "host")
		handleHost(var, iss, config);
	else if (var == "index")
		handleIndex(iss, config);
	else if (var == "error_page")
		handleErrorPage(dir_args, config);
	else if (var == "client_max_body_size")
		handleClientSize(iss, config);
	else if (!var.empty())
		throw InvalidFormat("Config File: Unknown directive in server block.");
}

void	ParseConfig::handleClientSize(std::istringstream &iss, ServerConfig &config)
{
	if (config.getClientMaxBodySize() != 0)
		throw InvalidFormat("Config File: Duplicate client_max_body_size directive.");

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

void ParseConfig::handleHost(const std::string &var, std::istringstream &iss, ServerConfig &config)
{
	if (var == "host" && config.getHost())
		throw InvalidFormat("Config File: Duplicate host directive.");
	std::string value;
	if (!(iss >> value))
		throw InvalidFormat("Config File: Missing value for host.");

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
	freeaddrinfo(res);
}

void	ParseConfig::handleListen(std::istringstream &iss, ServerConfig &config)
{
	if (config.getPort() != 0)
		throw InvalidFormat("Config File: Duplicate listen directive.");

	int value;
	if (!(iss >> value) || value < 0 || value > 65535)
		throw InvalidFormat("Config File: Invalid port number.");
	config.setPort(static_cast<unsigned short>(value));
}

void	ParseConfig::handleSimpleDirective(const std::string &var, std::istringstream &iss, ServerConfig &config)
{
	if ((var == "server_name" && !config.getServerName().empty()) ||
		(var == "root" && !config.getRoot().empty()))
		throw InvalidFormat("Config File: Duplicate " + var + " directive.");

	std::string value;
	if (!(iss >> value))
		throw InvalidFormat("Config File: Missing value for " + var + ".");
	if (var == "server_name") config.setServerName(value);
	else if (var == "root") config.setRoot(value);
}

void	ParseConfig::handleIndex(std::istringstream &iss, ServerConfig &config)
{
	if (!config.getIndex().empty())
		throw InvalidFormat("Config File: Duplicate index directive.");

	std::string value;
	while (iss >> value)
		config.addIndexBack(value);
}

void ParseConfig::handleErrorPage(std::string &line, ServerConfig &config)
{
	std::vector<std::string>	args;
	size_t	i = 0;
	while (i < line.size()) {
		while (i < line.size() && std::isspace(line[i]))
			i++;
		if (i >= line.size())
			break;

		if (line[i] == '\"' || line[i] == '\'') {
			char	quote = line[i++];
			size_t	start = i;
			while (i < line.size() && line[i] != quote)
				i++;
			if (i >= line.size())
				throw InvalidFormat("Config File: Invalid use of quotes in return directive.");
			std::string	token = line.substr(start, i - start);
			if (!token.empty())
				args.push_back(token);
			i++;
			if (i < line.size() && !std::isspace(line[i]))
				throw InvalidFormat("Config File: Invalid use of quotes in return directive.");
		}
		else {
			size_t	start = i;
			while (i < line.size() && !std::isspace(line[i]) && line[i] != '\"' && line[i] != '\'')
				i++;
			if (i < line.size() && (line[i] == '\"' || line[i] == '\''))
				throw InvalidFormat("Config File: Invalid use of quotes in return directive.");
			std::string	token = line.substr(start, i - start);
			if (!token.empty())
				args.push_back(token);
		}
		i++;
	}

	std::string	status_file = args.back();
	args.pop_back();

	for (size_t i = 0; i < args.size(); i++) {
		if (!isCode(args[i]) || !checkValidCode(std::atoi(args[i].c_str())))
			throw InvalidFormat("Config File: Invalid error code in error_page directive.");
		config.addErrorPageBack(std::atoi(args[i].c_str()), status_file);
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
