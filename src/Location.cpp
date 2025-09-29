#include "../inc/Location.hpp"

Location::Location() : autoindex(false), client_max_body_size(0) {}

Location::Location(const Location &other)
		: path(other.path),
		  root(other.root),
		  cgi_pass(other.cgi_pass),
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
		size_t first = conf_vec[i].find_first_not_of(" \t\n\r");
		if (std::string::npos == first) {
			trimmed_line = "";
		} else {
			size_t last = conf_vec[i].find_last_not_of(" \t\n\r");
			trimmed_line = conf_vec[i].substr(first, (last - first + 1));
		}

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

// helper function to parse the `location /path {` line
void	Location::parseDeclaration(std::vector<std::string> &conf_vec, size_t &i) {
	std::istringstream	ss(conf_vec[i]);
	std::string			keyword;
	std::string			token;

	ss >> keyword;
	if (keyword != "location")
		throw InvalidFormat("Config File: Invalid location declaration.");

	ss >> token;
	if (token == "~") {
		std::string	tmp;
		std::getline(ss, tmp, '{');

		size_t	first = tmp.find_first_not_of(" \t");
		size_t	last = tmp.find_last_not_of(" \t");
		if (first == std::string::npos)
			this->path = "";
		else
			this->path = token + " " + tmp.substr(first, last - first + 1);
		ss.putback('{');
	}
	else
		this->path = token;

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
	if (var == "cgi_pass") return DIR_CGI_PASS;
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
	size_t	semicolon_pos = line.find(';');
	if (line.empty() || semicolon_pos == std::string::npos)
		throw InvalidFormat("Config File: Missing ';' at end of line.");

	std::string after_semicolon = line.substr(semicolon_pos + 1);
	size_t comment_pos = after_semicolon.find('#');
	std::string trailing = (comment_pos != std::string::npos) ? after_semicolon.substr(0, comment_pos) : after_semicolon;
	for (size_t j = 0; j < trailing.size(); ++j) {
		if (!isspace(trailing[j]))
			throw InvalidFormat("Config File: Unexpected characters after ';'.");
	}

	std::istringstream iss(line.substr(0, semicolon_pos));
	std::string var;
	iss >> var;

	switch (getDirectiveType(var)) {
		case DIR_ROOT: {
			if (!this->root.empty())
				throw InvalidFormat("Config File: Duplicate root directive.");
			iss >> this->root;
			break;
		}
		case DIR_CGI_PASS: {
			if (!this->cgi_pass.empty())
				throw InvalidFormat("Config File: Duplicate cgi_pass directive.");
			iss >> this->cgi_pass;
			break;
		}
		case DIR_CGI_PATH: {
			if (!this->cgi_path.empty())
				throw InvalidFormat("Config File: Duplicate cgi_path directive.");
			iss >> this->cgi_path;
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
			if (!(iss >> this->return_dir.first >> this->return_dir.second))
				throw InvalidFormat("Config File: Invalid return directive.");
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
	std::swap(this->cgi_pass, other.cgi_pass);
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

std::string Location::getCgiPass() const {
	return this->cgi_pass;
}

std::string Location::getCgiPath() const {
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

std::pair<int, std::string> Location::getReturnDir() const {
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
