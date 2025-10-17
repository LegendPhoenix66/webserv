#include "../inc/Location.hpp"

Location::Location() : autoindex(false), client_max_body_size(0) {}

Location::Location(const Location &other)
		: path(other.path),
		  root(other.root),
		  cgi_ext(other.cgi_ext),
		  cgi_path(other.cgi_path),
		  index(other.index),
		  autoindex(other.autoindex),
		  allowed_methods(other.allowed_methods),
		  return_dir(other.return_dir),
		  upload_store(other.upload_store),
		  client_max_body_size(other.client_max_body_size) {}

Location	&Location::operator=(Location copy) {
	this->swap(copy);
	return *this;
}

Location::~Location() {}

Location::Location(std::vector<std::string> &conf_vec, size_t &i) : autoindex(false), client_max_body_size(0) {
	parseDeclaration(conf_vec, i);

	std::string trimmed_line;
	bool		end = false;
	while (++i < conf_vec.size()) {
		trimmed_line = trim(conf_vec[i]);

		if (trimmed_line.empty() || trimmed_line[0] == '#')
			continue;

		if (trimmed_line[0] == '}') {
			end = true;
			break;
		}

		parseDirective(trimmed_line);
	}

	if (!end)
		throw InvalidFormat("Config File: Missing '}' at end of location block.");
}

std::string	Location::trim(std::string line) {
	line.erase(0, line.find_first_not_of(" \t\n\r"));
	line.erase(line.find_last_not_of(" \t\n\r") + 1);
	return line;
}

// helper function to parse the `location /path {` line
void	Location::parseDeclaration(std::vector<std::string> &conf_vec, size_t &i) {
	std::istringstream	ss(conf_vec[i]);
	std::string			keyword;
	std::string			token;

	ss >> keyword;
	if (keyword != "location")
		throw InvalidFormat("Config File: Invalid location declaration.");

	ss >> this->path;
	if (this->path.empty() || this->path == "{")
		throw InvalidFormat("Config File: Missing or invalid path in location declaration.");

	ss >> token;
	if (token == "{")
		return;

	if (conf_vec[i].find('{') == std::string::npos) {
		i++;
		if (i >= conf_vec.size() || conf_vec[i].find('{') == std::string::npos)
			throw InvalidFormat("Config File: Missing '{' at start of location block.");
	}
}

Location::DirectiveType Location::getDirectiveType(const std::string &var) {
	if (var == "root") return DIR_ROOT;
	if (var == "cgi_ext") return DIR_CGI_EXT;
	if (var == "cgi_path") return DIR_CGI_PATH;
	if (var == "index") return DIR_INDEX;
	if (var == "autoindex") return DIR_AUTOINDEX;
	if (var == "allowed_methods") return DIR_ALLOWED_METHODS;
	if (var == "return") return DIR_RETURN;
	if (var == "upload_store") return DIR_UPLOAD_STORE;
	if (var == "client_max_body_size") return DIR_CLIENT_MAX_BODY_SIZE;
	if (var == "limit_except") return DIR_LIMIT_EXCEPT;
	if (var.empty()) return DIR_EMPTY;
	return DIR_UNKNOWN;
}

void	Location::parseDirective(const std::string &line) {
	const size_t	semicolon_pos = line.find(';');
	if (line.empty() || semicolon_pos == std::string::npos)
		throw InvalidFormat("Config File: Missing ';' at end of line.");

	const size_t	comment_pos = line.find('#', semicolon_pos);
	std::string trailing;
	if (comment_pos != std::string::npos)
		trailing = line.substr(semicolon_pos + 1, comment_pos - semicolon_pos - 1);
	else
		trailing = line.substr(semicolon_pos + 1);
	for (size_t j = 0; j < trailing.size(); j++) {
		if (!isspace(trailing[j]))
			throw InvalidFormat("Config File: Unexpected characters after ';'.");
	}

	std::istringstream iss(line.substr(0, semicolon_pos));
	std::string var;
	iss >> var;
	std::string	dir_args = trim(line.substr(var.size(), semicolon_pos - var.size()));

	switch (getDirectiveType(var)) {
		case DIR_ROOT: {
			if (!this->root.empty())
				throw InvalidFormat("Config File: Duplicate root directive.");
			iss >> this->root;
			break;
		}
		case DIR_CGI_EXT: {
			parseCGIExt(iss);
			break;
		}
		case DIR_CGI_PATH: {
			parseCGIPath(dir_args);
			break;
		}
		case DIR_INDEX:
			parseIndex(iss);
			break;
		case DIR_AUTOINDEX: {
			std::string value;
			iss >> value;
			if (value != "on" && value != "off")
				throw InvalidFormat("Config File: Invalid autoindex value.");
			this->autoindex = (value == "on");
			break;
		}
		case DIR_ALLOWED_METHODS:
			parseAllowedMethods(iss);
			break;
		case DIR_RETURN: {
			parseReturn(iss, dir_args);
			break;
		}
		case DIR_UPLOAD_STORE: {
			if (!this->upload_store.empty())
				throw InvalidFormat("Config File: Duplicate upload_store directive.");
			iss >> this->upload_store;
			break;
		}
		case DIR_CLIENT_MAX_BODY_SIZE:
			parseClientSize(iss);
			break;
		case DIR_LIMIT_EXCEPT: {
			if (!this->limit_except.empty())
				throw InvalidFormat("Config File: Duplicate limit_except directive.");
			std::string value;
			while (iss >> value)
				this->limit_except.push_back(value);
			break;
		}
		case DIR_EMPTY:
			break;
		default:
			throw InvalidFormat("Config File: Unknown directive in location block.");
	}
}

void Location::parseCGIPath(std::string &line) {
	std::vector<std::string>	paths;
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
				throw InvalidFormat("Config File: Invalid use of quotes in cgi_path directive.");
			std::string	token = line.substr(start, i - start);
			if (!token.empty())
				paths.push_back(token);
			i++;
			if (i < line.size() && !std::isspace(line[i]))
				throw InvalidFormat("Config File: Invalid use of quotes in cgi_path directive.");
		}
		else {
			size_t	start = i;
			while (i < line.size() && !std::isspace(line[i]) && line[i] != '\"' && line[i] != '\'')
				i++;
			if (i < line.size() && (line[i] == '\"' || line[i] == '\''))
				throw InvalidFormat("Config File: Invalid use of quotes in cgi_path directive.");
			std::string	token = line.substr(start, i - start);
			if (!token.empty())
				paths.push_back(token);
		}
		i++;
	}
	if (paths.empty())
		throw InvalidFormat("Config File: Missing arguments in cgi_path directive.");
	this->cgi_path = paths;
}

void	Location::parseCGIExt(std::istringstream &iss) {
	std::vector<std::string>	extensions;
	std::string					ext;

	while (iss >> ext) {
		if (ext.empty() || ext[0] != '.' || ext.find('.', 1) != std::string::npos)
			throw InvalidFormat("Config File: Invalid extension in cgi_ext directive.");
		extensions.push_back(ext);
	}
	if (extensions.empty())
		throw InvalidFormat("Config File: Missing arguments in cgi_ext directive.");
	this->cgi_ext = extensions;
}

bool	Location::isURL(const std::string &str) {
	if (str.find("http://") == 0 || str.find("https://") == 0)
		return true;
	return false;
}

void Location::parseReturn(std::istringstream &iss, std::string &line) {
	if (hasReturnDir())
		throw InvalidFormat("Config File: Duplicate return directives.");

	std::string	value;

	if (!(iss >> value) || !isCode(value) || !checkValidCode(std::atoi(value.c_str())))
		throw InvalidFormat("Config File: First argument of \"return\" must be a valid status code.");
	this->return_dir.code = std::atoi(value.c_str());

	std::string	args_str = trim(line.substr(value.size()));
	if (args_str.empty())
		return;

	std::vector<std::string>	args;
	size_t	i = 0;
	while (i < args_str.size()) {
		while (i < args_str.size() && std::isspace(args_str[i]))
			i++;
		if (i >= args_str.size())
			break;

		if (args_str[i] == '\"' || args_str[i] == '\'') {
			char	quote = args_str[i++];
			size_t	start = i;
			while (i < args_str.size() && args_str[i] != quote)
				i++;
			if (i >= args_str.size())
				throw InvalidFormat("Config File: Invalid use of quotes in return directive.");
			std::string	token = args_str.substr(start, i - start);
			if (!token.empty())
				args.push_back(token);
			i++;
			if (i < args_str.size() && !std::isspace(args_str[i]))
				throw InvalidFormat("Config File: Invalid use of quotes in return directive.");
		}
		else {
			size_t	start = i;
			while (i < args_str.size() && !std::isspace(args_str[i]) && args_str[i] != '\"' && args_str[i] != '\'')
				i++;
			if (i < args_str.size() && (args_str[i] == '\"' || args_str[i] == '\''))
				throw InvalidFormat("Config File: Invalid use of quotes in return directive.");
			std::string	token = args_str.substr(start, i - start);
			if (!token.empty())
				args.push_back(token);
		}
		i++;
	}

	if (args.size() > 2)
		throw InvalidFormat("Config File: Invalid return directive.");

	for (size_t j = 0; j < args.size(); j++) {
		if (isURL(args[j])) {
			if (!this->return_dir.url.empty())
				throw InvalidFormat("Config File: Invalid return directive.");
			this->return_dir.url = args[j];
		}
		else
			this->return_dir.text.push_back(args[j]);
	}
}

void	Location::parseClientSize(std::istringstream &iss) {
	if (this->client_max_body_size != 0)
		throw InvalidFormat("Config File: Duplicate client_max_body_size directive.");

	std::string value_str;
	if (!(iss >> value_str))
		throw InvalidFormat("Config File: Missing value for client_max_body_size.");

	char *endptr;
	long value = std::strtol(value_str.c_str(), &endptr, 10);
	if (value < 0 || (value == 0 && endptr == value_str.c_str()))
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

	this->client_max_body_size = static_cast<size_t>(value) * multiplier;
	if (iss >> value_str)
		throw InvalidFormat("Config File: client_max_body_size directive requires only one argument.");
}

// helper to handle index parsing
void Location::parseIndex(std::istringstream &iss) {
	if (!this->index.empty())
		throw InvalidFormat("Config File: Duplicate index directive.");
	std::string value;
	while (iss >> value)
		this->index.push_back(value);
}

// helper to handle allowed_methods parsing
void Location::parseAllowedMethods(std::istringstream &iss) {
	if (!this->allowed_methods.empty())
		throw InvalidFormat("Config File: Duplicate allowed_methods directive.");
	std::string value;
	while (iss >> value)
		this->allowed_methods.push_back(value);
}

void Location::swap(Location &other) {
	std::swap(this->path, other.path);
	std::swap(this->root, other.root);
	std::swap(this->cgi_ext, other.cgi_ext);
	std::swap(this->cgi_path, other.cgi_path);
	std::swap(this->index, other.index);
	std::swap(this->autoindex, other.autoindex);
	std::swap(this->allowed_methods, other.allowed_methods);
	std::swap(this->return_dir, other.return_dir);
	std::swap(this->upload_store, other.upload_store);
	std::swap(this->client_max_body_size, other.client_max_body_size);
}

std::string Location::getPath() const {
	return this->path;
}

std::string Location::getRoot() const {
	return this->root;
}

std::vector<std::string> Location::getCgiExt() const {
	return this->cgi_ext;
}

std::vector<std::string> Location::getCgiPath() const {
	return this->cgi_path;
}

std::vector<std::string> Location::getIndex() const {
	return this->index;
}

bool Location::getAutoindex() const {
	return this->autoindex;
}

std::vector<std::string> Location::getAllowedMethods() const {
	return this->allowed_methods;
}

ReturnDir Location::getReturnDir() const {
	return this->return_dir;
}

std::string Location::getUploadStore() const {
	return this->upload_store;
}

size_t	Location::getClientMaxBodySize() const {
	return this->client_max_body_size;
}

Location::InvalidFormat::InvalidFormat(std::string message) : message(message)
{}

Location::InvalidFormat::~InvalidFormat() throw()
{}

const char *Location::InvalidFormat::what() const throw() {
	return this->message.c_str();
}

bool	Location::hasReturnDir() {
	if (this->return_dir.code || !this->return_dir.url.empty() || !this->return_dir.text.empty())
		return true;
	return false;
}
