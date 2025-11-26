#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
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

struct DirCmp {
	std::string	fsPath;
	DirCmp(const std::string &p) : fsPath(p) {}

	bool	operator()(const std::string &a, const std::string &b) const;
};

bool		file_exists(const std::string &path, bool *isDir);
bool		read_file(const std::string &path, std::string &out);
std::string	join_path_absolute(const std::string &a, const std::string &b);
std::string	sanitize(const std::string &target);
std::string	html_escape(const std::string &s);
bool		dirCmp(const std::string &a, const std::string &b);
bool		generate_autoindex_tree(const std::string &fsPath, const std::string &urlPath, std::string &body, bool deleteMethod);
std::string	getFilefromExt(const std::string &target, const std::string &root, const std::string &ext);
std::string	peer_of(int fd);
std::string	normalize_target_simple(const std::string &t);
std::string	base_name_only(const std::string &p);
std::string	gen_unique_upload_name();
std::string	safe_filename(const std::string &s);
std::string join_path_relative(const std::string &a, const std::string &b);
std::string find_header_icase(const std::map<std::string, std::string> &hdrs, const std::string &name);
std::string strip_port(const std::string &host);
std::string to_lower_copy(const std::string &s);
