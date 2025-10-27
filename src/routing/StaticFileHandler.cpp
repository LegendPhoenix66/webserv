#include "../../inc/StaticFileHandler.hpp"
#include "../../inc/HttpRequest.hpp"
#include "../../inc/HttpResponse.hpp"
#include "../../inc/MimeTypes.hpp"

#include <sys/stat.h>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <dirent.h>

static bool file_exists(const std::string &path, bool *isDir) {
    struct stat st;
    if (::stat(path.c_str(), &st) == -1) return false;
    if (isDir) *isDir = S_ISDIR(st.st_mode);
    return S_ISREG(st.st_mode) || S_ISDIR(st.st_mode);
}

static bool read_file(const std::string &path, std::string &out) {
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

static std::string join_path(const std::string &a, const std::string &b) {
    if (a.empty()) return b;
    if (a[a.size()-1] == '/') return a + (b.size() && b[0] == '/' ? b.substr(1) : b);
    return a + "/" + (b.size() && b[0] == '/' ? b.substr(1) : b);
}

static std::string sanitize(const std::string &target) {
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

static std::string html_escape(const std::string &s) {
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

static bool generate_autoindex_body(const std::string &fsPath, const std::string &urlPath, std::string &body) {
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

namespace StaticFileHandler {

bool handle(const std::string &root,
            const std::vector<std::string> &indexList,
            const HttpRequest &req,
            bool isHead,
            bool autoindex,
            HttpResponse &outResp,
            std::string &err) {
    std::string clean = sanitize(req.target);
    std::string path = join_path(root, clean);

    bool isDir = false;
    if (!file_exists(path, &isDir)) {
        err = "not found";
        return false; // 404
    }

    if (isDir) {
        // Try index files in order
        for (size_t i = 0; i < indexList.size(); ++i) {
            std::string idx = join_path(path, indexList[i]);
            bool isDir2 = false;
            if (file_exists(idx, &isDir2) && !isDir2) {
                path = idx;
                isDir = false;
                break;
            }
        }
        // If still directory, maybe autoindex
        bool isDirFinal = false;
        (void)file_exists(path, &isDirFinal);
        if (isDirFinal) {
            if (!autoindex) {
                err = "index not found";
                return false;
            }
            std::string body;
            if (!generate_autoindex_body(path, clean, body)) {
                err = "autoindex generation failed";
                return false;
            }
            outResp.setStatus(200, "OK");
            outResp.setHeader("Connection", "close");
            outResp.setHeader("Content-Type", "text/html; charset=utf-8");
            {
                std::ostringstream oss; oss << body.size();
                outResp.setHeader("Content-Length", oss.str());
            }
            if (!isHead) outResp.setBody(body);
            return true;
        }
    }

    std::string body;
    long contentLen = 0;
    struct stat st;
    if (::stat(path.c_str(), &st) == 0) {
        contentLen = static_cast<long>(st.st_size);
    }
    if (!isHead) {
        if (!read_file(path, body)) {
            err = std::string("read error: ") + std::strerror(errno);
            return false; // treat as 404/500; for v0 we’ll do 404
        }
        contentLen = static_cast<long>(body.size());
    }

    outResp.setStatus(200, "OK");
    outResp.setHeader("Connection", "close");
    outResp.setHeader("Content-Type", MimeTypes::of(path));
    {
        std::ostringstream oss; oss << contentLen;
        outResp.setHeader("Content-Length", oss.str());
    }
    if (!isHead) outResp.setBody(body);
    return true;
}

} // namespace StaticFileHandler
