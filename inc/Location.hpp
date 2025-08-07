#ifndef LOCATION_HPP
#define LOCATION_HPP

#include "WebServ.hpp"

class Location
{
private:
	std::string					path;
	std::string					root;
	std::string					proxy_pass;
	std::string					cgi_path;
	std::vector<std::string>	index;
	bool						autoindex;
	std::vector<std::string>	allowed_methods;
	std::pair<int, std::string>	return_dir;
	std::string					upload_store;
public:
	Location();
	Location(const Location &copy);
	Location	operator=(const Location &copy);
	~Location();
	void		LocationParse(std::ifstream file);
};


#endif
