#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <ctime>
#include <cstdio>
#include "HttpStatusCodes.hpp"

class HttpResponse {
private:
	HttpStatusCode::e					_status_code;
	std::map<std::string, std::string>	_headers;
	std::string							_body;

	std::string	dateNow() const;
public:
	HttpResponse();
	HttpResponse(HttpStatusCode::e status_code);

	void	setStatus(HttpStatusCode::e status_code);
	void	setHeader(const std::string &name, const std::string &value);
	void	setBody(const std::string &body);

	// Serialize to a byte vector.
	std::vector<char>	serialize() const;
};

#endif
