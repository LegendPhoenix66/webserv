#ifndef LOCATION_HPP
#define LOCATION_HPP

#include "WebServ.hpp"

class Location
{
private:
	std::string					path;
	std::string					root;
	std::string					cgi_pass;
	std::string					cgi_path;
	std::vector<std::string>	index;
	bool						autoindex;
	std::vector<std::string>	allowed_methods;
	std::pair<int, std::string>	return_dir;
	std::string					upload_store;

	void swap(Location& other);
public:
	Location();
	Location(const Location &copy);
	Location(std::ifstream &file, std::string line);
	Location&   operator=(const Location &copy);
	~Location();

	std::string					getPath() const;
	std::string					getRoot() const;
	std::string					getCgiPass() const;
	std::string					getCgiPath() const;
	std::vector<std::string>	getIndex() const;
	bool						getAutoindex() const;
	std::vector<std::string>	getAllowedMethods() const;
	std::pair<int, std::string>	getReturnDir() const;
	std::string					getUploadStore() const;

	class	InvalidFormat : public std::exception {
	public:
		const char	*what() const throw();
	};
};


#endif
