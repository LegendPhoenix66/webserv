#include "../inc/ConnectionUtils.hpp"

bool file_exists(const std::string &path, bool *isDir) {
	struct stat st;
	if (::stat(path.c_str(), &st) == -1) return false;
	if (isDir) *isDir = S_ISDIR(st.st_mode);
	return S_ISREG(st.st_mode) || S_ISDIR(st.st_mode);
}

bool read_file(const std::string &path, std::string &out) {
	FILE *f = std::fopen(path.c_str(), "rb");
	if (!f) return false;
	std::string buf;
	char tmp[4096];
	size_t n;
	while ((n = std::fread(tmp, 1, sizeof(tmp), f)) > 0) buf.append(tmp, n);
	std::fclose(f);
	out.swap(buf);
	return true;
}

std::string join_path(const std::string &a, const std::string &b) {
	if (a.empty()) return b;
	if (a[a.size()-1] == '/') return a + (b.size() && b[0] == '/' ? b.substr(1) : b);
	return a + "/" + (b.size() && b[0] == '/' ? b.substr(1) : b);
}

std::string sanitize(const std::string &target) {
	// Ensure leading '/'; prevent ".." segments; collapse multiple '/'
	std::string out;
	out.reserve(target.size());
	if (target.empty() || target[0] != '/') out.push_back('/');
	for (size_t i = 0; i < target.size(); ++i) {
		char c = target[i];
		if (c == '\\') c = '/';
		out.push_back(c);
	}
	// Simple traversal prevention
	if (out.find("..") != std::string::npos) return "/"; // fallback to root
	return out;
}

std::string html_escape(const std::string &s) {
	std::string o; o.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		char c = s[i];
		if (c == '&') o += "&amp;";
		else if (c == '<') o += "&lt;";
		else if (c == '>') o += "&gt;";
		else if (c == '"') o += "&quot;";
		else o.push_back(c);
	}
	return o;
}

bool generate_autoindex_body(const std::string &fsPath, const std::string &urlPath, std::string &body) {
	DIR *dir = opendir(fsPath.c_str());
	if (!dir) return false;
	std::ostringstream oss;
	oss << "<html><head><title>Index of " << html_escape(urlPath) << "</title></head><body>\n";
	oss << "<h1>Index of " << html_escape(urlPath) << "</h1>\n<ul>\n";
	struct dirent *de;
	while ((de = readdir(dir)) != 0) {
		const char *name = de->d_name;
		if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0))) continue; // skip . and ..
		std::string entryName(name);
		std::string href = urlPath;
		if (href.size() == 0 || href[href.size()-1] != '/') href += "/";
		href += entryName;
		// Detect directory for trailing slash
		std::string childPath = join_path(fsPath, entryName);
		bool isDir = false; (void)file_exists(childPath, &isDir);
		oss << "  <li><a href=\"" << html_escape(href + (isDir ? "/" : "")) << "\">"
			<< html_escape(entryName + (isDir ? "/" : "")) << "</a></li>\n";
	}
	closedir(dir);
	oss << "</ul>\n</body></html>\n";
	body = oss.str();
	return true;
}

std::string peer_of(int fd) {
	struct sockaddr_in sa; socklen_t sl = sizeof(sa);
	char buf[64];
	if (getpeername(fd, (struct sockaddr*)&sa, &sl) == 0) {
		const char *ip = inet_ntop(AF_INET, &sa.sin_addr, buf, sizeof(buf));
		std::ostringstream oss; oss << (ip ? ip : "?") << ":" << ntohs(sa.sin_port);
		return oss.str();
	}
	return std::string("-");
}

std::string normalize_target_simple(const std::string &t) {
	if (t.empty()) return std::string("/");
	std::string out; out.reserve(t.size()+1);
	if (t[0] != '/') out.push_back('/');
	for (size_t i = 0; i < t.size(); ++i) {
		char c = t[i];
		if (c == '\\') c = '/';
		out.push_back(c);
	}
	return out;
}

std::string base_name_only(const std::string &p) {
	if (p.empty()) return p;
	std::string::size_type pos = p.find_last_of("/\\");
	if (pos == std::string::npos) return p;
	return p.substr(pos + 1);
}

std::string gen_unique_upload_name() {
	struct timeval tv; gettimeofday(&tv, 0);
	std::ostringstream oss; oss << "upload-" << (unsigned long)tv.tv_sec << "-" << (unsigned long)tv.tv_usec << ".bin";
	return oss.str();
}

std::string safe_filename(const std::string &s) {
	if (s.empty()) return std::string("upload.bin");
	std::string out; out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		char c = s[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
		out.push_back(ok ? c : '_');
	}
	if (out.empty()) out = "upload.bin";
	return out;
}

std::string join_path_simple(const std::string &a, const std::string &b) {
	if (a.empty()) return b;
	if (a[a.size()-1] == '/') return a + (b.size() && b[0] == '/' ? b.substr(1) : b);
	return a + "/" + (b.size() && b[0] == '/' ? b.substr(1) : b);
}

std::string find_header_icase(const std::map<std::string, std::string> &hdrs, const std::string &name) {
	std::string lname = to_lower_copy(name);
	for (std::map<std::string, std::string>::const_iterator it = hdrs.begin(); it != hdrs.end(); ++it) {
		if (to_lower_copy(it->first) == lname) return it->second;
	}
	return std::string();
}

std::string strip_port(const std::string &host) {
	std::string::size_type pos = host.find(':');
	if (pos == std::string::npos) return host;
	return host.substr(0, pos);
}

std::string to_lower_copy(const std::string &s) {
	std::string out = s;
	for (size_t i = 0; i < out.size(); ++i) {
		char c = out[i];
		if (c >= 'A' && c <= 'Z') out[i] = static_cast<char>(c - 'A' + 'a');
	}
	return out;
}
