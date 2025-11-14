#include <iostream>
#include <string>
#include <map>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sstream>

bool		file_exists(const std::string &path, bool *isDir);
bool		read_file(const std::string &path, std::string &out);
std::string	join_path(const std::string &a, const std::string &b);
std::string	sanitize(const std::string &target);
std::string	html_escape(const std::string &s);
bool		generate_autoindex_body(const std::string &fsPath, const std::string &urlPath, std::string &body);
std::string	peer_of(int fd);
std::string	normalize_target_simple(const std::string &t);
std::string	base_name_only(const std::string &p);
std::string	gen_unique_upload_name();
std::string	safe_filename(const std::string &s);
std::string join_path_simple(const std::string &a, const std::string &b);
std::string find_header_icase(const std::map<std::string, std::string> &hdrs, const std::string &name);
std::string strip_port(const std::string &host);
std::string to_lower_copy(const std::string &s);
