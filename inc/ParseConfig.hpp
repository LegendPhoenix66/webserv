#ifndef PARSECONF_HPP
#define PARSECONF_HPP

#include "WebServ.hpp"
#include "ServerConfig.hpp"
#include "HttpStatusCodes.hpp"
#include "ParseUtils.hpp"
#include "InvalidFormat.hpp"

/**
 * @class ParseConfig
 * @brief Parses and loads server configuration from a file.
 *
 * The ParseConfig class reads configuration files, validates their format,
 * and populates a ServerConfig object with the parsed directives. It handles
 * parsing of server blocks, directives, and location blocks, and throws
 * exceptions for invalid formats or file errors.
 */
class ParseConfig {
private:
	/** @brief The parsed server configuration. */
	std::vector<ServerConfig>    configs;

	/**
	 * @brief Swaps the contents of this ParseConfig with another.
	 * @param other The ParseConfig to swap with.
	 */
	void swap(ParseConfig &other);

	/**
	 * @brief Parses the header of the configuration file (e.g., "server {").
	 * @param fileStream The input file stream.
	 * @throws InvalidFormat if the header is invalid.
	 */
	static void parseHeader(std::vector<std::string> &conf_vec, size_t &i);

	/**
	 * @brief Parses the main configuration block.
	 * @param fileStream The input file stream.
	 * @throws InvalidFormat if the block is invalid.
	 */
	void parseConfigBlock(std::vector<std::string> &conf_vec, size_t &i);

	/**
	 * @brief Parses a single configuration directive line.
	 * @param conf_vec The input file stream.
	 * @param i The directive line to parse.
	 * @throws InvalidFormat if the directive is invalid.
	 */
	void parseDirective(std::vector<std::string> &conf_vec, size_t &i, ServerConfig &config);

	/**
	 * @brief Handles parsing of a location block.
	 * @param conf_vec The input file stream.
	 * @param i The location directive line.
	 * @throws InvalidFormat if the location block is invalid.
	 */
	void handleLocation(std::vector<std::string> &conf_vec, size_t &i, ServerConfig &config);

	/**
	 * @brief Handles the 'listen' directive.
	 * @param iss The input string stream containing the port value.
	 * @throws InvalidFormat if the port is invalid.
	 */
	void handleListen(std::istringstream &iss, ServerConfig &config);

	/**
	 * @brief Handles simple directives (server_name, root, host).
	 * @param var The directive name.
	 * @param var The input string stream containing the value.
	 * @throws InvalidFormat if the directive is invalid.
	 */
	void handleRoot(const std::string var, const std::string line, ServerConfig &config);

	/**
	 * @brief Handles the 'index' directive.
	 * @param iss The input string stream containing index file names.
	 */
	void handleIndex(const std::string var, const std::string line, ServerConfig &config);

	/**
	 * @brief Handles the 'error_page' directive.
	 * @param iss The input string stream containing error codes and URL.
	 * @throws InvalidFormat if the directive is invalid.
	 */
	void handleErrorPage(std::string var, std::string line, ServerConfig &config);

	/**
	 * @brief Handles the 'client_max_body_size' directive.
	 * @param iss The input string stream containing the size value.
	 * @throws InvalidFormat if the size is invalid.
	 */
	void handleClientSize(std::istringstream &iss, ServerConfig &config);

	void handleHost(std::istringstream &iss, ServerConfig &config);

	void	checkBrackets(const std::vector<std::string> conf_vec);

public:
	/**
	 * @brief Default constructor.
	 */
	ParseConfig();

	/**
	 * @brief Copy constructor.
	 * @param copy The ParseConfig to copy from.
	 */
	ParseConfig(const ParseConfig &copy);

	/**
	 * @brief Assignment operator using copy-and-swap idiom.
	 * @param copy The ParseConfig to assign from.
	 * @return Reference to this ParseConfig.
	 */
	ParseConfig &operator=(ParseConfig copy);

	/**
	 * @brief Destructor.
	 */
	~ParseConfig();

	/**
	 * @brief Constructs a ParseConfig by parsing configuration from a file.
	 * @param file The filename to parse.
	 * @throws CouldNotOpenFile if the file cannot be opened.
	 * @throws InvalidFormat if the configuration is invalid.
	 */
	explicit ParseConfig(std::string file);

	/**
	 * @brief Gets the parsed server configuration.
	 * @return The ServerConfig object.
	 */
	std::vector<ServerConfig>	getConfigs() const;

	/**
	 * @class CouldNotOpenFile
	 * @brief Exception thrown when a configuration file cannot be opened.
	 */
	class CouldNotOpenFile : public std::exception {
	public:
		/**
		 * @brief Returns an error message describing the file open error.
		 * @return The error message string.
		 */
		const char *what() const throw();
	};

	class IsDirectoryError :public std::exception {
	public:
		const char *what() const throw();
	};
};

#endif
