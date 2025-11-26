#ifndef LOCATION_HPP
#define LOCATION_HPP

#include "WebServ.hpp"
#include "HttpStatusCodes.hpp"
#include "ParseUtils.hpp"
#include "InvalidFormat.hpp"

struct ReturnDir {
	int							code;
	std::string					url;
	ReturnDir() : code(0) {}
};

class Location {
private:
	std::string					path;
	std::string					root;
	std::string					cgi_pass;
	std::string					cgi_path;
	std::string					cgi_ext;
	std::vector<std::string>	index;
	std::vector<std::string>	allowed_methods;
	ReturnDir					return_dir;
	std::string					upload_store;
	bool						autoindex;
	long long					client_max_body_size;

	void	swap(Location &other);
	enum DirectiveType {
		DIR_ROOT,           /**< The 'root' directive. */
		DIR_CGI_PASS,       /**< The 'cgi_pass' directive. */
		DIR_CGI_PATH,       /**< The 'cgi_path' directive. */
		DIR_CGI_EXT,
		DIR_INDEX,          /**< The 'index' directive. */
		DIR_ALLOWED_METHODS,/**< The 'allowed_methods' directive. */
		DIR_RETURN,         /**< The 'return' directive. */
		DIR_UPLOAD_STORE,   /**< The 'upload_store' directive. */
		DIR_CLIENT_MAX_BODY_SIZE, /**< The 'client_max_body_size' directive. */
		DIR_AUTOINDEX,
		DIR_EMPTY,
		DIR_UNKNOWN         /**< Unknown or unsupported directive. */
	};

	static DirectiveType getDirectiveType(const std::string &var);

	void	parseDirective(const std::string &line);
	void	parseDeclaration(std::vector<std::string> &conf_vec, size_t &i);
	void	parseIndex(const std::string var, const std::string line);
	void	parseAllowedMethods(std::istringstream &iss);
	void	parseClientSize(std::istringstream &iss);
	void	parseReturn(std::istringstream &iss, const std::string var, const std::string line);
	void	parseCgiExt(std::istringstream &iss);

public:
	Location();
	Location(const Location &other);
	Location &operator=(Location copy);
	~Location();
	Location(std::vector<std::string> &conf_vec, size_t &i);
	bool	hasReturnDir() const;
	std::string	getPath() const;
	std::string	getRoot() const;
	std::string	getCgiPass() const;
	std::string	getCgiPath() const;
	std::string	getCgiExt() const;
	std::vector<std::string>	getIndex() const;
	std::vector<std::string>	getAllowedMethods() const;
	std::string					getMethod(const std::string method) const;
	std::string::size_type		findMethod(const std::string &method) const;
	ReturnDir	getReturnDir() const;
	std::string	getUploadStore() const;
	long long	getClientMaxBodySize() const;
	bool	getAutoindex() const;
};


#endif
