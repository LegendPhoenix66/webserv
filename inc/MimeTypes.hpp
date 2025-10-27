#ifndef MIME_TYPES_HPP
#define MIME_TYPES_HPP

#include <string>

namespace MimeTypes {
    // Return a MIME type for a given filename (by extension). Defaults to application/octet-stream.
    std::string of(const std::string &path);
}

#endif // MIME_TYPES_HPP
