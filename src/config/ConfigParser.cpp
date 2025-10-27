#include "../../inc/ConfigParser.hpp"
#include <vector>
#include <fstream>
#include <sstream>
#include <cctype>
#include <iostream>

namespace {

struct Token {
    enum Type { T_IDENT, T_NUMBER, T_STRING, T_LBRACE, T_RBRACE, T_SEMI, T_EOF } type;
    std::string text; int line; int col;
    Token(Type t=T_EOF, const std::string &s="", int l=1, int c=1) : type(t), text(s), line(l), col(c) {}
};

class Lexer {
public:
    Lexer(const std::string &src, const std::string &file)
        : _src(src), _i(0), _line(1), _col(1), _file(file) {}

    Token next() {
        skipWSAndComments();
        if (eof()) return Token(Token::T_EOF, "", _line, _col);
        char ch = peek();
        if (ch == '{') { advance(); return Token(Token::T_LBRACE, "{", _line, _col-1); }
        if (ch == '}') { advance(); return Token(Token::T_RBRACE, "}", _line, _col-1); }
        if (ch == ';') { advance(); return Token(Token::T_SEMI, ";", _line, _col-1); }
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            int l=_line, c=_col; std::string num;
            while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) { num += peek(); advance(); }
            return Token(Token::T_NUMBER, num, l, c);
        }
        if (isIdentStart(ch)) {
            int l=_line, c=_col; std::string id;
            while (!eof() && isIdentCont(peek())) { id += peek(); advance(); }
            return Token(Token::T_IDENT, id, l, c);
        }
        // Treat any non-structural non-space sequence as STRING
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            int l=_line, c=_col; std::string s;
            while (!eof()) {
                char p = peek();
                if (std::isspace(static_cast<unsigned char>(p)) || p=='{' || p=='}' || p==';' || p=='#') break;
                s += p; advance();
            }
            return Token(Token::T_STRING, s, l, c);
        }
        // Fallback
        int l=_line, c=_col; advance();
        return Token(Token::T_STRING, std::string(1,ch), l, c);
    }

    const std::string &file() const { return _file; }

private:
    std::string _src; size_t _i; int _line; int _col; std::string _file;

    bool eof() const { return _i >= _src.size(); }
    char peek() const { return _src[_i]; }
    void advance() { if (_src[_i]=='\n'){_line++; _col=1;} else {_col++;} _i++; }

    static bool isIdentStart(char ch) {
        return std::isalpha(static_cast<unsigned char>(ch)) || ch=='_' || ch=='/' || ch=='.' || ch=='~';
    }
    static bool isIdentCont(char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) || ch=='_' || ch=='-' || ch=='.' || ch=='/' || ch=='~';
    }

    void skipWSAndComments() {
        while (!eof()) {
            char ch = peek();
            if (std::isspace(static_cast<unsigned char>(ch))) { advance(); continue; }
            if (ch == '#') { // comment till end of line
                while (!eof() && peek()!='\n') advance();
                continue;
            }
            break;
        }
    }
};

class Parser {
public:
    Parser(Lexer &lex, const std::string &file) : _lex(lex), _file(file) { _look = _lex.next(); }

    Config parse() {
        Config cfg;
        while (_look.type != Token::T_EOF) {
            expectIdent("server");
            expect(Token::T_LBRACE);
            ServerConfig srv = parseServerBlock();
            cfg.servers.push_back(srv);
        }
        if (cfg.servers.empty()) {
            throw ConfigError(_file, 1, 1, "no server blocks found", SYNTAX_ERROR);
        }
        validate(cfg);
        return cfg;
    }

private:
    Lexer &_lex; Token _look; std::string _file;

    void advance() { _look = _lex.next(); }
    void expect(Token::Type t) {
        if (_look.type != t) {
            std::ostringstream oss; oss << "unexpected token '" << _look.text << "'";
            throw ConfigError(_file, _look.line, _look.col, oss.str(), SYNTAX_ERROR);
        }
        advance();
    }
    void expectIdent(const std::string &name) {
        if (!(_look.type == Token::T_IDENT && _look.text == name)) {
            std::ostringstream oss; oss << "expected '" << name << "'";
            throw ConfigError(_file, _look.line, _look.col, oss.str(), SYNTAX_ERROR);
        }
        advance();
    }

    bool accept(Token::Type t) { if (_look.type==t){ advance(); return true;} return false; }

    // Read a scalar value composed of IDENT/STRING/NUMBER tokens until a semicolon.
    // Throws with a directive-specific message if the first token is not acceptable.
    std::string readScalarUntilSemicolon(const char* expectedMsg) {
        if (!(_look.type == Token::T_IDENT || _look.type == Token::T_STRING || _look.type == Token::T_NUMBER)) {
            throw ConfigError(_file, _look.line, _look.col, expectedMsg, SYNTAX_ERROR);
        }
        std::string out;
        while (_look.type == Token::T_IDENT || _look.type == Token::T_STRING || _look.type == Token::T_NUMBER) {
            out += _look.text;
            advance();
        }
        expect(Token::T_SEMI);
        return out;
    }

    // Skip an unsupported directive or block even when arguments precede '{'.
    void skipUnknown(const std::string &directive) {
        int l = _look.line, c = _look.col;
        // Consume tokens until we hit ';' (simple directive) or '{' (block start) or a terminator.
        while (_look.type != Token::T_SEMI && _look.type != Token::T_LBRACE && _look.type != Token::T_EOF && _look.type != Token::T_RBRACE) {
            advance();
        }
        if (_look.type == Token::T_SEMI) {
            std::cerr << _lex.file() << ":" << l << ":" << c << ": warning: skipping unsupported directive '" << directive << "'\n";
            advance();
            return;
        }
        if (_look.type == Token::T_LBRACE) {
            std::cerr << _lex.file() << ":" << l << ":" << c << ": warning: skipping unsupported block '" << directive << "'\n";
            skipBlock();
            return;
        }
        // If EOF/RBRACE encountered, we simply return and let caller handle structure.
        std::cerr << _lex.file() << ":" << l << ":" << c << ": warning: incomplete/unsupported directive '" << directive << "'\n";
    }

    ServerConfig parseServerBlock() {
        ServerConfig s;
        while (_look.type != Token::T_RBRACE) {
            if (_look.type == Token::T_EOF) {
                throw ConfigError(_file, _look.line, _look.col, "unexpected end of file inside server block", SYNTAX_ERROR);
            }
            if (_look.type != Token::T_IDENT) {
                std::ostringstream oss; oss << "unexpected token '" << _look.text << "' in server block";
                throw ConfigError(_file, _look.line, _look.col, oss.str(), SYNTAX_ERROR);
            }
            std::string directive = _look.text;
            advance();
            if (directive == "listen") {
                if (_look.type != Token::T_NUMBER) {
                    throw ConfigError(_file, _look.line, _look.col, "listen expects a port number", SYNTAX_ERROR);
                }
                int port = toInt(_look.text);
                s.port = port; s.has_port = true; advance();
                expect(Token::T_SEMI);
            } else if (directive == "host") {
                s.host = readScalarUntilSemicolon("host expects an address/name");
                s.has_host = true;
            } else if (directive == "root") {
                if (!(_look.type == Token::T_IDENT || _look.type == Token::T_STRING)) {
                    throw ConfigError(_file, _look.line, _look.col, "root expects a path", SYNTAX_ERROR);
                }
                s.root = _look.text; s.has_root = true; advance();
                expect(Token::T_SEMI);
            } else if (directive == "index") {
                while (_look.type == Token::T_IDENT || _look.type == Token::T_STRING) {
                    s.index.push_back(_look.text);
                    advance();
                }
                expect(Token::T_SEMI);
            } else if (directive == "error_page") {
                std::vector<int> codes;
                while (_look.type == Token::T_NUMBER) { codes.push_back(toInt(_look.text)); advance(); }
                if (!(_look.type == Token::T_IDENT || _look.type == Token::T_STRING)) {
                    throw ConfigError(_file, _look.line, _look.col, "error_page expects a path after codes", SYNTAX_ERROR);
                }
                std::string path = _look.text; advance();
                for (std::vector<int>::iterator it=codes.begin(); it!=codes.end(); ++it) {
                    s.error_pages[*it] = path;
                }
                expect(Token::T_SEMI);
            } else if (directive == "server_name") {
                while (_look.type == Token::T_IDENT || _look.type == Token::T_STRING) { s.server_names.push_back(_look.text); advance(); }
                expect(Token::T_SEMI);
            } else if (directive == "client_max_body_size") {
                if (_look.type != Token::T_NUMBER) {
                    throw ConfigError(_file, _look.line, _look.col, "client_max_body_size expects a number", SYNTAX_ERROR);
                }
                s.client_max_body_size = toInt(_look.text);
                advance();
                expect(Token::T_SEMI);
            } else if (directive == "max_header_size") {
                if (_look.type != Token::T_NUMBER) {
                    throw ConfigError(_file, _look.line, _look.col, "max_header_size expects a number (bytes)", SYNTAX_ERROR);
                }
                s.max_header_size = toInt(_look.text);
                advance();
                expect(Token::T_SEMI);
            } else if (directive == "max_header_lines") {
                if (_look.type != Token::T_NUMBER) {
                    throw ConfigError(_file, _look.line, _look.col, "max_header_lines expects a number", SYNTAX_ERROR);
                }
                s.max_header_lines = toInt(_look.text);
                advance();
                expect(Token::T_SEMI);
            } else if (directive == "max_request_line") {
                if (_look.type != Token::T_NUMBER) {
                    throw ConfigError(_file, _look.line, _look.col, "max_request_line expects a number (bytes)", SYNTAX_ERROR);
                }
                s.max_request_line = toInt(_look.text);
                advance();
                expect(Token::T_SEMI);
            } else if (directive == "header_timeout_ms") {
                if (_look.type != Token::T_NUMBER) {
                    throw ConfigError(_file, _look.line, _look.col, "header_timeout_ms expects a number (milliseconds)", SYNTAX_ERROR);
                }
                s.header_timeout_ms = toInt(_look.text);
                advance();
                expect(Token::T_SEMI);
            } else if (directive == "location") {
                // Expect a path (IDENT or STRING) then a block
                if (!(_look.type == Token::T_IDENT || _look.type == Token::T_STRING)) {
                    throw ConfigError(_file, _look.line, _look.col, "location expects a path starting with '/'", SYNTAX_ERROR);
                }
                std::string lpath = _look.text; advance();
                if (lpath.empty() || lpath[0] != '/') {
                    throw ConfigError(_file, _look.line, _look.col, "location path must start with '/'", SYNTAX_ERROR);
                }
                if (_look.type != Token::T_LBRACE) {
                    if (_look.type == Token::T_SEMI) {
                        // treat as empty directive (unexpected); skip
                        advance();
                        continue;
                    }
                    // If there are extra tokens, skip until '{' or ';'
                    while (_look.type != Token::T_LBRACE && _look.type != Token::T_SEMI && _look.type != Token::T_EOF && _look.type != Token::T_RBRACE) advance();
                }
                if (_look.type == Token::T_SEMI) { advance(); continue; }
                expect(Token::T_LBRACE);
                LocationConfig loc; loc.path = lpath;
                // Parse location block body
                while (_look.type != Token::T_RBRACE) {
                    if (_look.type == Token::T_EOF) {
                        throw ConfigError(_file, _look.line, _look.col, "unexpected end of file inside location block", SYNTAX_ERROR);
                    }
                    if (_look.type != Token::T_IDENT) {
                        std::ostringstream oss; oss << "unexpected token '" << _look.text << "' in location block";
                        throw ConfigError(_file, _look.line, _look.col, oss.str(), SYNTAX_ERROR);
                    }
                    std::string ldir = _look.text; advance();
                    if (ldir == "allowed_methods") {
                        unsigned mask = 0;
                        while (_look.type == Token::T_IDENT) {
                            std::string m = _look.text; advance();
                            if (m == "GET") mask |= 1u;
                            else if (m == "HEAD") mask |= 2u;
                            else if (m == "POST") mask |= 4u;
                            else if (m == "DELETE") mask |= 8u;
                            else {
                                std::cerr << _file << ": warning: unknown method '" << m << "' in allowed_methods; ignoring\n";
                            }
                        }
                        loc.methods_mask = mask; loc.has_methods = true; expect(Token::T_SEMI);
                    } else if (ldir == "root") {
                        if (!(_look.type == Token::T_IDENT || _look.type == Token::T_STRING)) {
                            throw ConfigError(_file, _look.line, _look.col, "root expects a path", SYNTAX_ERROR);
                        }
                        loc.root = _look.text; loc.has_root = true; advance(); expect(Token::T_SEMI);
                    } else if (ldir == "index") {
                        while (_look.type == Token::T_IDENT || _look.type == Token::T_STRING) { loc.index.push_back(_look.text); advance(); }
                        loc.has_index = true; expect(Token::T_SEMI);
                    } else if (ldir == "autoindex") {
                        if (!(_look.type == Token::T_IDENT || _look.type == Token::T_STRING)) {
                            throw ConfigError(_file, _look.line, _look.col, "autoindex expects 'on' or 'off'", SYNTAX_ERROR);
                        }
                        std::string v = _look.text; advance(); expect(Token::T_SEMI);
                        loc.autoindex = (v == "on" || v == "ON" || v == "On");
                        loc.has_autoindex = true;
                    } else if (ldir == "return") {
                        if (_look.type != Token::T_NUMBER) { throw ConfigError(_file, _look.line, _look.col, "return expects a status code", SYNTAX_ERROR); }
                        int code = toInt(_look.text); advance();
                        if (!(_look.type == Token::T_IDENT || _look.type == Token::T_STRING || _look.type == Token::T_NUMBER)) {
                            throw ConfigError(_file, _look.line, _look.col, "return expects a URL after status code", SYNTAX_ERROR);
                        }
                        std::string url = _look.text; advance(); expect(Token::T_SEMI);
                        if (code == 301 || code == 302) {
                            loc.redirect_code = code; loc.redirect_location = url; loc.has_redirect = true;
                        } else {
                            std::cerr << _file << ": warning: unsupported return code " << code << "; only 301/302 supported in this phase; ignoring\n";
                        }
                    } else if (ldir == "cgi_pass") {
                        if (!(_look.type == Token::T_IDENT || _look.type == Token::T_STRING)) { throw ConfigError(_file, _look.line, _look.col, "cgi_pass expects a path", SYNTAX_ERROR); }
                        loc.cgi_pass = _look.text; advance(); expect(Token::T_SEMI);
                    } else if (ldir == "cgi_path") {
                        if (!(_look.type == Token::T_IDENT || _look.type == Token::T_STRING)) { throw ConfigError(_file, _look.line, _look.col, "cgi_path expects a path", SYNTAX_ERROR); }
                        loc.cgi_path = _look.text; advance(); expect(Token::T_SEMI);
                    } else if (ldir == "upload_store") {
                        if (!(_look.type == Token::T_IDENT || _look.type == Token::T_STRING)) { throw ConfigError(_file, _look.line, _look.col, "upload_store expects a path", SYNTAX_ERROR); }
                        loc.upload_store = _look.text; advance(); expect(Token::T_SEMI);
                    } else if (ldir == "client_max_body_size") {
                        if (_look.type != Token::T_NUMBER) { throw ConfigError(_file, _look.line, _look.col, "client_max_body_size expects a number", SYNTAX_ERROR); }
                        loc.client_max_body_size_override = toInt(_look.text); advance(); expect(Token::T_SEMI);
                    } else {
                        // unknown directive in location
                        skipUnknown(ldir);
                    }
                }
                expect(Token::T_RBRACE);
                s.locations.push_back(loc);
            } else {
                // Robust skipping for unknown directives/blocks (handles arguments before '{').
                skipUnknown(directive);
            }
        }
        expect(Token::T_RBRACE);
        return s;
    }

    void skipBlock() {
        // assumes current token is '{'
        expect(Token::T_LBRACE);
        int depth = 1;
        while (depth > 0) {
            if (_look.type == Token::T_EOF) {
                throw ConfigError(_file, _look.line, _look.col, "unexpected end of file while skipping block", SYNTAX_ERROR);
            }
            if (_look.type == Token::T_LBRACE) { depth++; advance(); continue; }
            if (_look.type == Token::T_RBRACE) { depth--; advance(); continue; }
            advance();
        }
    }

    void skipUntilSemicolon() {
        while (_look.type != Token::T_SEMI && _look.type != Token::T_EOF && _look.type != Token::T_RBRACE) advance();
        if (_look.type == Token::T_SEMI) advance();
    }

    static int toInt(const std::string &s) {
        std::istringstream iss(s); int v=0; iss >> v; return v;
    }

    void validate(const Config &cfg) {
        if (cfg.servers.empty()) return;

        // helper: lowercase copy (ASCII)
        struct Lower {
            static std::string down(const std::string &s) {
                std::string o = s;
                for (size_t i = 0; i < o.size(); ++i) {
                    char c = o[i];
                    if (c >= 'A' && c <= 'Z') o[i] = static_cast<char>(c - 'A' + 'a');
                }
                return o;
            }
        };

        // Track duplicate server_name per bind key (host:port)
        std::map<std::string, std::map<std::string, size_t> > seenNames; // bindKey -> name -> first index

        for (size_t i = 0; i < cfg.servers.size(); ++i) {
            const ServerConfig &s = cfg.servers[i];

            // Required fields for every server
            if (!s.has_host) {
                std::ostringstream oss; oss << "validation: missing required 'host' in server #" << (i + 1);
                throw ConfigError(_file, 0, 0, oss.str(), VALIDATION_ERROR);
            }
            if (!s.has_port) {
                std::ostringstream oss; oss << "validation: missing required 'listen' in server #" << (i + 1);
                throw ConfigError(_file, 0, 0, oss.str(), VALIDATION_ERROR);
            }
            if (!s.has_root) {
                std::ostringstream oss; oss << "validation: missing required 'root' in server #" << (i + 1);
                throw ConfigError(_file, 0, 0, oss.str(), VALIDATION_ERROR);
            }
            if (s.port < 1 || s.port > 65535) {
                std::ostringstream oss; oss << "validation: listen port out of range (1..65535) in server #" << (i + 1);
                throw ConfigError(_file, 0, 0, oss.str(), VALIDATION_ERROR);
            }

            // Validate error codes range for this server
            for (std::map<int,std::string>::const_iterator it = s.error_pages.begin(); it != s.error_pages.end(); ++it) {
                if (it->first < 100 || it->first > 599) {
                    std::ostringstream oss; oss << "validation: error_page code out of range (100..599) in server #" << (i + 1);
                    throw ConfigError(_file, 0, 0, oss.str(), VALIDATION_ERROR);
                }
            }

            // Duplicate server_name warnings within the same bind group
            const std::string bindKey = bindKeyOf(s);
            for (size_t j = 0; j < s.server_names.size(); ++j) {
                std::string nm = Lower::down(s.server_names[j]);
                if (nm.empty()) continue;
                std::map<std::string, size_t> &bucket = seenNames[bindKey];
                std::map<std::string, size_t>::const_iterator it = bucket.find(nm);
                if (it == bucket.end()) {
                    bucket[nm] = i; // remember first server index declaring this name for this bind
                } else {
                    size_t firstIdx = it->second;
                    std::cerr << _file << ": warning: duplicate server_name '" << s.server_names[j]
                              << "' for bind " << bindKey
                              << " (first declared in server #" << (firstIdx + 1)
                              << "; first declaration will win at runtime)\n";
                }
            }

            // Validate locations
            if (!s.locations.empty()) {
                std::map<std::string, size_t> seenLocs; // path -> first index
                for (size_t j = 0; j < s.locations.size(); ++j) {
                    const LocationConfig &loc = s.locations[j];
                    if (loc.path.empty() || loc.path[0] != '/') {
                        std::ostringstream oss; oss << "validation: location path must start with '/' in server #" << (i + 1);
                        throw ConfigError(_file, 0, 0, oss.str(), VALIDATION_ERROR);
                    }
                    std::map<std::string, size_t>::const_iterator it2 = seenLocs.find(loc.path);
                    if (it2 == seenLocs.end()) {
                        seenLocs[loc.path] = j;
                    } else {
                        size_t firstJ = it2->second;
                        std::cerr << _file << ": warning: duplicate location '" << loc.path << "' in server #" << (i + 1)
                                  << " (first declared at index " << (firstJ + 1) << "; first declaration will win)\n";
                    }
                }
            }
        }
    }
};

} // anonymous

ConfigParser::ConfigParser() {}

Config ConfigParser::parseFile(const std::string &path) {
    std::ifstream ifs(path.c_str()); // ensure C++98 friendliness
    if (!ifs) {
        throw ConfigError(path, 0, 0, "cannot open configuration file", IO_ERROR);
    }
    std::ostringstream ss; ss << ifs.rdbuf();
    std::string src = ss.str();
    Lexer lex(src, path);
    Parser parser(lex, path);
    return parser.parse();
}
