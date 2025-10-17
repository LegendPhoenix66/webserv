#include "../inc/InvalidFormat.hpp"

InvalidFormat::InvalidFormat(std::string message) : message("Config File: " + message)
{}

InvalidFormat::~InvalidFormat() throw()
{}

const char	*InvalidFormat::what() const throw() {
	return this->message.c_str();
}
