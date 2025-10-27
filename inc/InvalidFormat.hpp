#ifndef INVALIDFORMAT_HPP
#define INVALIDFORMAT_HPP

#include "WebServ.hpp"

class InvalidFormat : public std::exception {
private:
	std::string	message;
public:
	/**
	 * @brief Returns an error message describing the invalid format.
	 * @return The error message string.
	 */
	explicit InvalidFormat(std::string message = "Invalid format.");
	~InvalidFormat() throw();
	const char *what() const throw();
};

#endif
