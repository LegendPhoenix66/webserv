#include "../inc/ConnectionUtils.hpp"

bool file_exists(const std::string &path, bool *isDir) {
	struct stat st;
	if (::stat(path.c_str(), &st) == -1) return false;
	if (isDir) *isDir = S_ISDIR(st.st_mode);
	return S_ISREG(st.st_mode) || S_ISDIR(st.st_mode);
}

bool read_file(const std::string &path, std::string &out) {
	std::ifstream	f(path.c_str(), std::ios::binary);
	if (!f) return false;
	std::string buf;
	char tmp[4096];
	while (f.read(tmp, sizeof(tmp)) || f.gcount() > 0)
		buf.append(tmp, f.gcount());
	f.close();
	out.swap(buf);
	return true;
}

std::string sanitize(const std::string &target) {
	// Ensure leading '/'; prevent ".." segments; collapse multiple '/'
	std::string out;
	std::string::size_type	end = target.find_first_of("?#");
	if (end == std::string::npos) end = target.size();
	out.reserve(end + 1);

	if (target.empty() || target[0] != '/') out.push_back('/');
	for (std::string::size_type i = 0; i < end; i++) {
		char c = target[i];
		if (c == '\\') c = '/';
		if (!out.empty() && out[out.size() - 1] == '/' && c == '/') continue;
		out.push_back(c);
	}
	if (out.empty() || out[0] != '/') out.insert(out.begin(), '/');

	std::string::size_type	pos = 0;
	if (out.size() > 0 && out[0] == '/') pos = 1;
	while (pos <= out.size()) {
		std::string::size_type	next = out.find('/', pos);
		std::string::size_type	segLen;
		segLen = (next == std::string::npos) ? out.size() - pos : next - pos;
		if (segLen == 2 && pos + 1 < out.size() && out[pos] == '.' && out[pos + 1] == '.')
			return std::string("/");
		if (next == std::string::npos) break;
		pos = next + 1;
	}
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

bool	DirCmp::operator()(const std::string &a, const std::string &b) const {
	std::string	fullA = join_path_relative(fsPath, a);
	std::string	fullB = join_path_relative(fsPath, b);
	bool	isDirA = false;
	bool	isDirB = false;
	file_exists(fullA, &isDirA);
	file_exists(fullB, &isDirB);
	if (isDirA != isDirB) return isDirA;
	return a < b;
}

bool	generate_autoindex_tree(const std::string &fsPath, const std::string &urlPath, std::string &body, bool deleteMethod) {
	DIR *dir = opendir(fsPath.c_str());
	if (!dir) return false;

	std::ostringstream	oss;
	oss << "<html><head><title>Index of " << html_escape(urlPath) << "</title></head><body>\n";
	oss << "<h1>Index of " << html_escape(urlPath) << "</h1>\n<ul style=list-style:none>\n";

	struct dirent *de;
	std::vector<std::string>	entries;

	while ((de = readdir(dir)) != 0) {
		const char *name = de->d_name;
		if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0)))
			continue;
		entries.push_back(std::string(name));
	}
	closedir(dir);

	std::sort(entries.begin(), entries.end(), DirCmp(fsPath));

	if (urlPath != "/" && !urlPath.empty()) {
		std::string parentPath = urlPath;
		if (parentPath[parentPath.size() - 1] == '/')
			parentPath = parentPath.substr(0, parentPath.size() - 1);
		std::string::size_type	lastSlash = parentPath.find_last_of('/');
		if (lastSlash != std::string::npos) {
			parentPath = parentPath.substr(0, lastSlash + 1);
			if (parentPath.empty()) parentPath = "/";
		}
		oss << "<li style=\"padding:.2rem 0\">";
		oss << "<a href=\"" << html_escape(parentPath) << "\"><- PARENT DIRECTORY</a></li>\n";
	}

	for (size_t i = 0; i < entries.size(); i++) {
		std::string	childPath = join_path_relative(fsPath, entries[i]);
		bool	isDir = false;
		file_exists(childPath, &isDir);

		std::string	href = urlPath;
		if (href.size() == 0 || href[href.size() - 1] != '/') href += "/";
		href += entries[i];

		oss << "<li style=\"padding:.2rem 0\">" << (isDir ? "[DIR]  " : "[FILE] ");
		oss << "<a href=\"" << html_escape(href + (isDir ? "/" : "")) << "\">";
		oss << html_escape(entries[i] + (isDir ? "/" : "")) << "</a>";
		if (!isDir) {
			std::string	dlHref = href;
			dlHref += (dlHref.find('?') != std::string::npos) ? "&__download=1" : "?__download=1";
			oss << " <a href=\"" << html_escape(dlHref) << "\">[DOWNLOAD]</a>";
		}
		if (!isDir && deleteMethod) {
			std::string	delHref = href;
			delHref += (delHref.find('?') != std::string::npos) ? "&__method=DELETE" : "?__method=DELETE";
			oss << " <a href=\"" << html_escape(delHref) << "\">[DELETE]</a>";
		}
		oss << "\n";
	}

	oss << "</li>\n</body></html>\n";

	body = oss.str();
	return true;
}

std::string	getFilefromExt(const std::string &target, const std::string &root, const std::string &ext) {
	if (ext.empty()) return std::string();

	std::string	scriptLoc = target;
	std::string::size_type	qpos = scriptLoc.find('?');
	if (qpos != std::string::npos) scriptLoc.erase(qpos);
	std::string	scriptPath = join_path_relative(root, scriptLoc);

	bool	isDir = false;
	if (!(file_exists(scriptPath, &isDir) && !isDir)) return std::string();

	if (scriptPath.size() > ext.size() && scriptPath.compare(scriptPath.size() - ext.size(), ext.size(), ext) == 0)
		return scriptPath;
	return std::string();
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

std::string join_path_absolute(const std::string &a, const std::string &b) {
	if (!b.empty() && b[0] == '/') return b;
	if (a.empty()) {
		if (b.empty()) return std::string(".");
		if (b[0] == '/' || b.find("./") == 0) return b;
		return std::string("./") + b;
	}
	if (a[a.size()-1] == '/') return a + (b.size() && b[0] == '/' ? b.substr(1) : b);
	return a + "/" + (b.size() && b[0] == '/' ? b.substr(1) : b);
}

std::string join_path_relative(const std::string &a, const std::string &b) {
	if (a.empty()) {
		if (b.empty()) return std::string(".");
		if (b[0] == '/') return b;
		return std::string("./") + b;
	}
	std::string	out = a;
	if (!out.empty() && out[out.size() - 1] != '/') out += '/';
	if (!b.empty()) {
		std::string	nb = b;
		if (nb[0] == '.') nb.erase(0, 1);
		if (nb[0] == '/') nb.erase(0, 1);
		out += nb;
	}
	if (out.empty()) out = ".";
	return out;
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
