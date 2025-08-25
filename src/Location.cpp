#include "../inc/Location.hpp"

Location::Location() : autoindex(false) {}

Location::Location(const Location &other)
        : path(other.path),
          root(other.root),
          cgi_pass(other.cgi_pass),
          cgi_path(other.cgi_path),
          index(other.index),
          autoindex(other.autoindex),
          allowed_methods(other.allowed_methods),
          return_dir(other.return_dir),
          upload_store(other.upload_store) {}

Location &Location::operator=(Location copy) {
    this->swap(copy);
    return *this;
}

Location::~Location() {}

Location::Location(std::ifstream &file, std::string line) : autoindex(false) {
    parseDeclaration(file, line);

    std::string trimmed_line;
    while (std::getline(file, line)) {
        size_t first = line.find_first_not_of(" \t\n\r");
        if (std::string::npos == first) {
            trimmed_line = "";
        } else {
            size_t last = line.find_last_not_of(" \t\n\r");
            trimmed_line = line.substr(first, (last - first + 1));
        }

        if (trimmed_line.empty() || trimmed_line[0] == '#')
            continue;

        if (trimmed_line[0] == '}')
            break;

        parseDirective(trimmed_line);
    }
}

// helper function to parse the `location /path {` line
void Location::parseDeclaration(std::ifstream &file, std::string &line) {
    std::istringstream ss(line);
    std::string keyword;

    ss >> keyword;
    if (keyword != "location")
        throw InvalidFormat();

    ss >> this->path;
    if (this->path.empty() || this->path == "{")
        throw InvalidFormat();

    if (line.find('{') == std::string::npos) {
        if (!std::getline(file, line) || line.find('{') == std::string::npos) {
            throw InvalidFormat();
        }
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
    return DIR_UNKNOWN;
}

void Location::parseDirective(const std::string &line) {
    if (line.empty() || line[line.size() - 1] != ';')
        throw InvalidFormat();

    std::istringstream iss(line.substr(0, line.size() - 1));
    std::string var;
    iss >> var;

    switch (getDirectiveType(var)) {
        case DIR_ROOT:
            iss >> this->root;
            break;
        case DIR_CGI_PASS:
            iss >> this->cgi_pass;
            break;
        case DIR_CGI_PATH:
            iss >> this->cgi_path;
            break;
        case DIR_INDEX:
            parseIndex(iss);
            break;
        case DIR_AUTOINDEX: {
            std::string value;
            iss >> value;
            if (value != "on" && value != "off")
                throw InvalidFormat();
            this->autoindex = (value == "on");
            break;
        }
        case DIR_ALLOWED_METHODS:
            parseAllowedMethods(iss);
            break;
        case DIR_RETURN:
            if (!(iss >> this->return_dir.first >> this->return_dir.second))
                throw InvalidFormat();
            break;
        case DIR_UPLOAD_STORE:
            iss >> this->upload_store;
            break;
		case DIR_CLIENT_MAX_BODY_SIZE: {
			int value;
			if (!(iss >> value) || value < 0)
				throw InvalidFormat();
			this->client_max_body_size = static_cast<size_t>(value);
			break;
		}
        default:
            throw InvalidFormat();
    }
}

// helper to handle index parsing
void Location::parseIndex(std::istringstream &iss) {
    std::string value;
    while (iss >> value)
        this->index.push_back(value);
}

// helper to handle allowed_methods parsing
void Location::parseAllowedMethods(std::istringstream &iss) {
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

const char *Location::InvalidFormat::what() const throw() {
    return "invalid format.";
}
