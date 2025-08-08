#include "../inc/Location.hpp"

Location::Location()
{
}

Location::Location(const Location &copy)
: path(copy.path),
  root(copy.root),
  cgi_pass(copy.cgi_pass),
  cgi_path(copy.cgi_path),
  index(copy.index),
  autoindex(copy.autoindex),
  allowed_methods(copy.allowed_methods),
  return_dir(copy.return_dir),
  upload_store(copy.upload_store)
{
}

Location::Location(std::ifstream &file, std::string line) : autoindex(false)
{
	std::istringstream	ss(line);
	std::string			keyword;

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

	while (std::getline(file, line)) {
		line.erase(0, line.find_first_not_of(" \t\n"));
		line.erase(line.find_last_not_of(" \t\n") + 1);

		if (line.empty() || line[0] == '#')
			continue;

		if (line[0] == '}')
			break;

		if (line[line.size() - 1] != ';')
			throw InvalidFormat();
		line.erase(line.size() - 1);

		std::istringstream	iss(line);
		std::string			var;
		iss >> var;

		if (var == "root")
			iss >> this->root;
		else if (var == "cgi_pass")
			iss >> this->cgi_pass;
		else if (var == "cgi_path")
			iss >> this->cgi_path;
		else if (var == "index") {
			std::string	value;
			while (iss >> value)
				this->index.push_back(value);
		}
		else if (var == "autoindex") {
			std::string	value;
			iss >> value;
			if (value != "on" && value != "off")
				throw InvalidFormat();
			this->autoindex = (value == "on");
		}
		else if (var == "allowed_methods") {
			std::string	value;
			while (iss >> value)
				this->allowed_methods.push_back(value);
		}
		else if (var == "return") {
			if (!(iss >> this->return_dir.first >> this->return_dir.second))
				throw InvalidFormat();
		}
		else if (var == "upload_store")
			iss >> this->upload_store;
		else
			throw InvalidFormat();
	}
}

Location	Location::operator=(const Location &copy)
{
	if (this != &copy) {
		*this = copy;
	}
	return *this;
}

Location::~Location()
{
}

std::string	Location::getPath() const
{
	return this->path;
}

std::string	Location::getRoot() const
{
	return this->root;
}

std::string	Location::getCgiPass() const
{
	return this->cgi_pass;
}

std::string	Location::getCgiPath() const
{
	return this->cgi_path;
}

std::vector<std::string>	Location::getIndex() const
{
	return this->index;
}

bool	Location::getAutoindex() const
{
	return this->autoindex;
}

std::vector<std::string>	Location::getAllowedMethods() const
{
	return this->allowed_methods;
}

std::pair<int, std::string>	Location::getReturnDir() const
{
	return this->return_dir;
}

std::string	Location::getUploadStore() const
{
	return this->upload_store;
}

const char	*Location::InvalidFormat::what() const throw()
{
	return "invalid format.";
}
