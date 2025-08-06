#include "../inc/Location.hpp"

Location::Location()
{
}

Location::Location(const Location &copy)
:	path(copy.path),
	root(copy.root),
	proxy_pass(copy.proxy_pass),
	cgi_path(copy.cgi_path),
	index(copy.index),
	autoindex(copy.autoindex),
	allowed_methods(copy.allowed_methods),
	return_dir(copy.return_dir),
	upload_store(copy.upload_store)
{
}

Location	Location::operator=(const Location &copy)
{
	if (this != &copy) {
		this->path = copy.path;
		this->root = copy.root;
		this->proxy_pass = copy.proxy_pass;
		this->cgi_path = copy.cgi_path;
		this->index = copy.index;
		this->autoindex = copy.autoindex;
		this->allowed_methods = copy.allowed_methods;
		this->return_dir = copy.return_dir;
		this->upload_store = copy.upload_store;
	}
	return *this;
}

Location::~Location()
{
}
