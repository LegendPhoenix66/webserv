#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "WebServ.hpp"
#include "Location.hpp"

/**
 * @class ServerConfig
 * @brief Represents the configuration for a single web server instance.
 *
 * The ServerConfig class encapsulates all configuration directives for a server,
 * including port, server name, host, root directory, index files, location blocks,
 * error page mappings, and client body size limits. It provides setters, getters,
 * and utility methods for managing and accessing these options.
 */
class ServerConfig {
private:
	/** @brief Port number the server listens on. */
	uint16_t port;
	/** @brief Name of the server (used for virtual hosting). */
	std::string server_name;
	/** @brief Host address for the server. */
	uint32_t	host;
	/** @brief Root directory for serving files. */
	std::string root;
	/** @brief List of index files to use when serving directories. */
	std::vector<std::string> index;
	/** @brief List of location blocks for request routing. */
	std::vector<Location> locations;
	/** @brief Mapping of HTTP error codes to custom error page paths. */
	std::map<int, std::string> error_pages;
	/** @brief Maximum allowed size for client request bodies (in bytes). */
	size_t	client_max_body_size;

	/**
	 * @brief Swaps the contents of this ServerConfig with another.
	 * @param other The ServerConfig to swap with.
	 */
	void swap(ServerConfig &other);

public:
	/**
	 * @brief Default constructor. Initializes all fields to default values.
	 */
	ServerConfig();

	/**
	 * @brief Copy constructor.
	 * @param copy The ServerConfig to copy from.
	 */
	ServerConfig(const ServerConfig &copy);

	/**
	 * @brief Assignment operator using copy-and-swap idiom.
	 * @param copy The ServerConfig to assign from.
	 * @return Reference to this ServerConfig.
	 */
	ServerConfig &operator=(ServerConfig copy);

	/**
	 * @brief Destructor.
	 */
	~ServerConfig();

	/**
	 * @brief Sets the port number for the server.
	 * @param port The port to set.
	 */
	void setPort(uint16_t port);

	/**
	 * @brief Sets the server name (for virtual hosting).
	 * @param name The server name.
	 */
	void setServerName(std::string name);

	/**
	 * @brief Sets the host address for the server.
	 * @param host The host address.
	 */
	void setHost(uint32_t host);

	/**
	 * @brief Sets the root directory for serving files.
	 * @param root The root directory.
	 */
	void setRoot(std::string root);

	/**
	 * @brief Sets the maximum allowed size for client request bodies.
	 * @param size The size in bytes.
	 */
	void setClientMaxBodySize(size_t size);

	/**
	 * @brief Adds an index file to the list of index files.
	 * @param index The index file name.
	 */
	void addIndexBack(const std::string &index);

	/**
	 * @brief Adds a location block for request routing.
	 * @param loc The Location object to add.
	 */
	void addLocationBack(const Location &loc);

	/**
	 * @brief Adds a custom error page mapping for a specific HTTP error code.
	 * @param code The error code.
	 * @param url The URL or path to the error page.
	 */
	void addErrorPageBack(int code, std::string url);

	/**
	 * @brief Gets the port number the server listens on.
	 * @return The port number.
	 */
	uint16_t getPort() const;

	/**
	 * @brief Gets the server name.
	 * @return The server name string.
	 */
	std::string getServerName() const;

	/**
	 * @brief Gets the host address.
	 * @return The host address string.
	 */
	uint32_t getHost() const;

	/**
	 * @brief Gets the root directory for the server.
	 * @return The root directory string.
	 */
	std::string getRoot() const;

	/**
	 * @brief Gets the list of index files.
	 * @return A vector of index file names.
	 */
	std::vector<std::string> getIndex() const;

	/**
	 * @brief Gets the list of location blocks.
	 * @return A vector of Location objects.
	 */
	std::vector<Location> getLocations() const;

	/**
	 * @brief Gets the error page mappings.
	 * @return A map of error codes to error page URLs.
	 */
	std::map<int, std::string> getErrorPages() const;

	/**
	 * @brief Gets the maximum allowed client request body size.
	 * @return The client max body size in bytes.
	 */
	size_t getClientMaxBodySize() const;

	Location	findLocationForPath(std::string path) const;
};

#endif
