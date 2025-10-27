#include "../inc/Location.hpp"

Location::Location() : client_max_body_size(0), autoindex(false) {}

Location::Location(const Location &other)
		: path(other.path),
		  root(other.root),
		  cgi_ext(other.cgi_ext),
		  cgi_path(other.cgi_path),
		  index(other.index),
		  allowed_methods(other.allowed_methods),
		  return_dir(other.return_dir),
		  upload_path(other.upload_path),
		  autoindex(other.autoindex),
		  client_max_body_size(other.client_max_body_size) {}

Location	&Location::operator=(Location copy) {
	this->swap(copy);
	return *this;
}

Location::~Location() {}

Location::Location(std::vector<std::string> &conf_vec, size_t &i) : client_max_body_size(0) {
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
		throw InvalidFormat("Missing '}' at end of location block.");
}

// helper function to parse the `location /path {` line
void	Location::parseDeclaration(std::vector<std::string> &conf_vec, size_t &i) {
	std::istringstream	ss(conf_vec[i]);
	std::string			keyword;
	std::string			token;

	ss >> keyword;
	if (keyword != "location")
		throw InvalidFormat("Invalid location declaration.");

	ss >> this->path;
	if (this->path.empty() || this->path == "{")
		throw InvalidFormat("Missing or invalid path in location declaration.");
	if (this->path[this->path.size() - 1] == '/')
		this->path.erase(this->path.size() - 1);

	ss >> token;
	if (token == "{")
		return;

	if (conf_vec[i].find('{') == std::string::npos) {
		i++;
		if (i >= conf_vec.size() || conf_vec[i].find('{') == std::string::npos)
			throw InvalidFormat("Missing '{' at start of location block.");
	}
}

Location::DirectiveType Location::getDirectiveType(const std::string &var) {
	if (var == "root") return DIR_ROOT;
	if (var == "cgi_ext") return DIR_CGI_EXT;
	if (var == "cgi_path") return DIR_CGI_PATH;
	if (var == "index") return DIR_INDEX;
	if (var == "allowed_methods") return DIR_ALLOWED_METHODS;
	if (var == "return") return DIR_RETURN;
	if (var == "upload_path") return DIR_UPLOAD_PATH;
	if (var == "client_max_body_size") return DIR_CLIENT_MAX_BODY_SIZE;
	if (var == "limit_except") return DIR_LIMIT_EXCEPT;
	if (var.empty()) return DIR_EMPTY;
	return DIR_UNKNOWN;
}

void	Location::parseDirective(const std::string &line) {
	const size_t	semicolon_pos = findLineEnd(line);
	std::istringstream iss(line.substr(0, semicolon_pos));
	std::string var;
	iss >> var;
	std::string	dir_args = trim(line.substr(var.size(), semicolon_pos - var.size()));

	switch (getDirectiveType(var)) {
		case DIR_ROOT: {
			if (!this->root.empty())
				throw InvalidFormat("Duplicate root directive.");
			this->root = extractSinglePath(var, dir_args);
			break;
		}
		case DIR_CGI_EXT: {
			parseCGIExt(iss);
			break;
		}
		case DIR_CGI_PATH: {
			std::vector<std::string>	paths = extractQuotedArgs(var, dir_args);
			if (paths.empty())
				throw InvalidFormat("Missing arguments in cgi_path directive.");
			this->cgi_path = paths;
			break;
		}
		case DIR_INDEX:
			parseIndex(var, dir_args);
			break;
		case DIR_ALLOWED_METHODS:
			parseAllowedMethods(iss);
			break;
		case DIR_RETURN: {
			parseReturn(iss, var, dir_args);
			break;
		}
		case DIR_UPLOAD_PATH: {
			if (!this->upload_path.empty())
				throw InvalidFormat("Duplicate upload_path directive.");
			this->upload_path = extractSinglePath(var, dir_args);
			break;
		}
		case DIR_CLIENT_MAX_BODY_SIZE:
			parseClientSize(iss);
			break;
		case DIR_LIMIT_EXCEPT: {
			if (!this->limit_except.empty())
				throw InvalidFormat("Duplicate limit_except directive.");
			std::string value;
			while (iss >> value)
				this->limit_except.push_back(value);
			break;
		}
		case DIR_AUTOINDEX:
			std::string	value;
			iss >> value;
			if (value != "on" || value != "off")
				throw InvalidFormat("Invalid value for autoindex directive.");
			this->autoindex = (value == "on");
			if (iss >> value)
				throw InvalidFormat("autoindex directive requires only one argument.");
			break;
		case DIR_EMPTY:
			break;
		default:
			throw InvalidFormat("Unknown directive in location block.");
	}
}

void	Location::parseCGIExt(std::istringstream &iss) {
	std::vector<std::string>	extensions;
	std::string					ext;

	while (iss >> ext) {
		if (ext.empty() || ext[0] != '.' || ext.find('.', 1) != std::string::npos)
			throw InvalidFormat("Invalid extension in cgi_ext directive.");
		extensions.push_back(ext);
	}
	if (extensions.empty())
		throw InvalidFormat("Missing arguments in cgi_ext directive.");
	this->cgi_ext = extensions;
}

bool	Location::isURL(const std::string &str) {
	if (str.find("http://") == 0 || str.find("https://") == 0)
		return true;
	return false;
}

void Location::parseReturn(std::istringstream &iss, const std::string var, const std::string line)
{
	if (hasReturnDir())
		throw InvalidFormat("Duplicate return directives.");

	std::string	value;

	if (!(iss >> value) || !isCode(value) || !checkValidCode(std::atoi(value.c_str())))
		throw InvalidFormat("First argument of \"return\" must be a valid status code.");
	this->return_dir.code = std::atoi(value.c_str());

	std::string	args_str = trim(line.substr(value.size()));
	if (args_str.empty())
		return;

	std::vector<std::string>	args = extractQuotedArgs(var, line);

	if (args.size() > 2)
		throw InvalidFormat("Invalid return directive.");

	for (size_t j = 0; j < args.size(); j++) {
		if (isURL(args[j])) {
			if (!this->return_dir.url.empty())
				throw InvalidFormat("Invalid return directive.");
			this->return_dir.url = args[j];
		}
		else
			this->return_dir.text.push_back(args[j]);
	}
}

void	Location::parseClientSize(std::istringstream &iss) {
	if (this->client_max_body_size != 0)
		throw InvalidFormat("Duplicate client_max_body_size directive.");

	std::string value_str;
	if (!(iss >> value_str))
		throw InvalidFormat("Missing value for client_max_body_size.");

	char *endptr;
	long value = std::strtol(value_str.c_str(), &endptr, 10);
	if (value < 0 || (value == 0 && endptr == value_str.c_str()))
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

	this->client_max_body_size = static_cast<size_t>(value) * multiplier;
	if (iss >> value_str)
		throw InvalidFormat("client_max_body_size directive requires only one argument.");
}

// helper to handle index parsing
void Location::parseIndex(const std::string var, const std::string line) {
	if (!this->index.empty())
		throw InvalidFormat("Duplicate index directive.");
	this->index = extractQuotedArgs(var, line);
}

// helper to handle allowed_methods parsing
void Location::parseAllowedMethods(std::istringstream &iss) {
	if (!this->allowed_methods.empty())
		throw InvalidFormat("Duplicate allowed_methods directive.");
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
	std::swap(this->allowed_methods, other.allowed_methods);
	std::swap(this->return_dir, other.return_dir);
	std::swap(this->upload_path, other.upload_path);
	std::swap(this->autoindex, other.autoindex);
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

std::vector<std::string> Location::getAllowedMethods() const {
	return this->allowed_methods;
}

std::string	Location::findMethod(const std::string &method) const {
	for (size_t i = 0; i < this->allowed_methods.size(); i++) {
		if (this->allowed_methods[i] == method)
			return this->allowed_methods[i];
	}
	return "";
}

ReturnDir Location::getReturnDir() const {
	return this->return_dir;
}

std::string Location::getUploadStore() const {
	return this->upload_path;
}

size_t	Location::getClientMaxBodySize() const {
	return this->client_max_body_size;
}

bool	Location::hasReturnDir() {
	if (this->return_dir.code || !this->return_dir.url.empty() || !this->return_dir.text.empty())
		return true;
	return false;
}
