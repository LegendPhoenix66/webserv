#ifndef LOCATION_HPP
#define LOCATION_HPP

#include "WebServ.hpp"
#include "HttpStatusCodes.hpp"
#include "ParseUtils.hpp"
#include "InvalidFormat.hpp"

struct ReturnDir {
	int							code;
	std::string					url;
	std::vector<std::string>	text;
	ReturnDir() : code(0) {}
};

/**
 * @class Location
 * @brief Represents a configuration block for a specific location in a web server.
 *
 * The Location class encapsulates configuration directives for a particular path
 * within the server. It stores settings such as the location path, root directory,
 * CGI parameters, index files, allowed HTTP methods, autoindex flag, return directives,
 * and upload storage directory. This class provides parsing utilities and accessors
 * for these configuration options, enabling fine-grained control over server behavior
 * for different URL locations.
 */
class Location {
private:
	/** @brief The path for which this location block applies (e.g., "/images"). */
	std::string path;
	/** @brief The root directory for this location. */
	std::string root;
	/** @brief The CGI handler to use for this location. */
	std::vector<std::string>	cgi_ext;
	/** @brief The path to the CGI executable. */
	std::vector<std::string>	cgi_path;
	/** @brief List of index files for this location. */
	std::vector<std::string> index;
	/** @brief List of allowed HTTP methods (e.g., GET, POST). */
	std::vector<std::string> allowed_methods;
	/** @brief Return directive: pair of status code and URL. */
	ReturnDir	return_dir;
	/** @brief Directory for file uploads in this location. */
	std::string upload_path;
	/** @brief Maximum allowed size for client request bodies (in bytes). */
	size_t	client_max_body_size;
	std::vector<std::string>	limit_except;

	/**
	 * @brief Swaps the contents of this Location with another.
	 * @param other The Location to swap with.
	 */
	void swap(Location &other);

	/**
	 * @enum DirectiveType
	 * @brief Enumerates all supported configuration directive types for a Location block.
	 *
	 * Used to map directive strings to a specific type, enabling switch-based parsing.
	 */
	enum DirectiveType {
		DIR_ROOT,           /**< The 'root' directive. */
		DIR_CGI_EXT,       /**< The 'cgi_ext' directive. */
		DIR_CGI_PATH,       /**< The 'cgi_path' directive. */
		DIR_INDEX,          /**< The 'index' directive. */
		DIR_ALLOWED_METHODS,/**< The 'allowed_methods' directive. */
		DIR_RETURN,         /**< The 'return' directive. */
		DIR_UPLOAD_PATH,   /**< The 'upload_path' directive. */
		DIR_CLIENT_MAX_BODY_SIZE, /**< The 'client_max_body_size' directive. */
		DIR_LIMIT_EXCEPT,   /**< The 'limit_except' directive. */
		DIR_EMPTY,
		DIR_UNKNOWN         /**< Unknown or unsupported directive. */
	};

	/**
	 * @brief Maps a directive string to its corresponding DirectiveType enum value.
	 * @param var The directive name as a string.
	 * @return The corresponding DirectiveType value, or DIR_UNKNOWN if not recognized.
	 */
	static DirectiveType getDirectiveType(const std::string &var);

	/**
	* @brief Parses a single configuration directive line.
	* @param line The directive line to parse.
	* @throws InvalidFormat if the directive is invalid.
	*/
	void parseDirective(const std::string &line);

	/**
	 * @brief Parses the location declaration line (e.g., "location /path {").
	 * @param conf_vec The input file stream.
	 * @param i The current line to parse.
	 * @throws InvalidFormat if the declaration is invalid.
	 */
	void parseDeclaration(std::vector<std::string> &conf_vec, size_t &i);

	/**
	 * @brief Parses the index directive and populates the index vector.
	 * @param iss The input string stream containing index values.
	 */
	void parseIndex(std::istringstream &iss);

	/**
	 * @brief Parses the allowed_methods directive and populates the allowed_methods vector.
	 * @param iss The input string stream containing method names.
	 */
	void parseAllowedMethods(std::istringstream &iss);

	void	parseClientSize(std::istringstream &iss);
	void	parseReturn(std::istringstream &iss, const std::string var, const std::string line);
	void	parseCGIExt(std::istringstream &iss);
	bool	isURL(const std::string &str);

public:
	/**
	 * @brief Default constructor. Initializes autoindex to false.
	 */
	Location();

	/**
	 * @brief Copy constructor.
	 * @param other The Location to copy from.
	 */
	Location(const Location &other);

	/**
	 * @brief Assignment operator using copy-and-swap idiom.
	 * @param copy The Location to assign from.
	 * @return Reference to this Location.
	 */
	Location &operator=(Location copy);

	/**
	 * @brief Destructor.
	 */
	~Location();

	/**
	 * @brief Constructs a Location by parsing configuration from a file stream.
	 * @param conf_vec The input file stream.
	 * @param i The current line to start parsing from.
	 * @throws InvalidFormat if the configuration is invalid.
	 */
	Location(std::vector<std::string> &conf_vec, size_t &i);

	bool	hasReturnDir();

	/**
	 * @brief Gets the location path.
	 * @return The path string.
	 */
	std::string getPath() const;

	/**
	 * @brief Gets the root directory.
	 * @return The root directory string.
	 */
	std::string getRoot() const;

	/**
	 * @brief Gets the CGI handler.
	 * @return The CGI handler string.
	 */
	std::vector<std::string> getCgiExt() const;

	/**
	 * @brief Gets the CGI executable path.
	 * @return The CGI path string.
	 */
	std::vector<std::string> getCgiPath() const;

	/**
	 * @brief Gets the list of index files.
	 * @return A vector of index file names.
	 */
	std::vector<std::string> getIndex() const;

	/**
	 * @brief Gets the list of allowed HTTP methods.
	 * @return A vector of allowed method names.
	 */
	std::vector<std::string> getAllowedMethods() const;

	/**
	 * @brief Gets the return directive.
	 * @return A pair of status code and URL.
	 */
	ReturnDir getReturnDir() const;

	/**
	 * @brief Gets the upload storage directory.
	 * @return The upload store string.
	 */
	std::string getUploadStore() const;

	/**
	 * @brief Gets the maximum allowed size for client request bodies.
	 * @return The size in bytes.
	 */
	size_t getClientMaxBodySize() const;
};


#endif
