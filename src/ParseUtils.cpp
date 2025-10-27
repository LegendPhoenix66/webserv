#include "../inc/ParseUtils.hpp"

std::vector<std::string>	extractQuotedArgs(const std::string var, const std::string line) {
	std::vector<std::string>	args;
	size_t	i = 0;
	while (i < line.size()) {
		while (i < line.size() && std::isspace(line[i]))
			i++;
		if (i >= line.size())
			break;

		if (line[i] == '\"' || line[i] == '\'') {
			char	quote = line[i++];
			size_t	start = i;
			while (i < line.size() && line[i] != quote)
				i++;
			if (i >= line.size())
				throw InvalidFormat("Invalid use of quotes in " + var + " directive.");
			std::string	token = line.substr(start, i - start);
			if (!token.empty())
				args.push_back(token);
			i++;
			if (i < line.size() && !std::isspace(line[i]))
				throw InvalidFormat("Invalid use of quotes in " + var + " directive.");
		}
		else {
			size_t	start = i;
			while (i < line.size() && !std::isspace(line[i]) && line[i] != '\"' && line[i] != '\'')
				i++;
			if (i < line.size() && (line[i] == '\"' || line[i] == '\''))
				throw InvalidFormat("Invalid use of quotes in " + var + " directive.");
			std::string	token = line.substr(start, i - start);
			if (!token.empty())
				args.push_back(token);
		}
		i++;
	}
	return args;
}

std::string	extractSinglePath(const std::string var, const std::string line)
{
	std::vector<std::string>	args = extractQuotedArgs(var, line);
	if (args.empty())
		throw InvalidFormat("Missing argument in " + var + " directive.");
	else if (args.size() > 1)
		throw InvalidFormat(var + " directive requires only one argument.");
	return args[0];
}

std::string	trim(std::string line) {
	line.erase(0, line.find_first_not_of(" \t\n\r"));
	line.erase(line.find_last_not_of(" \t\n\r") + 1);
	return line;
}

size_t	findLineEnd(const std::string line) {
	const size_t	semicolon_pos = line.find(';');
	if (line.empty() || semicolon_pos == std::string::npos)
		throw InvalidFormat("Missing ';' at end of line.");

	const size_t	comment_pos = line.find('#', semicolon_pos);
	std::string trailing;
	if (comment_pos != std::string::npos)
		trailing = line.substr(semicolon_pos + 1, comment_pos - semicolon_pos - 1);
	else
		trailing = line.substr(semicolon_pos + 1);
	for (size_t j = 0; j < trailing.size(); j++) {
		if (!isspace(trailing[j]))
			throw InvalidFormat("Unexpected characters after ';'.");
	}
	return semicolon_pos;
}

bool	isDirectory(const char *path) {
	DIR		*dir;

	dir = opendir(path);
	if (dir != NULL) {
		closedir(dir);
		return true;
	}
	return false;
}
