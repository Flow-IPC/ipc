#pragma once
#include "snap.hpp"
#include <string_view>
#include <vector>

namespace snap {

enum class HttpMethod { GET, POST, PUT, DEL, HEAD, OPT, TRC, CON, PAT, UNK };
enum class HttpStatus { OK = 200, CRT = 201, ACC = 202, NOC = 204, BAD = 400, UNA = 401, FOR = 403, NTF = 404, ERR = 500 };

// Header structure. I kept it as string_view to avoid data copy.
struct HttpHdr {
    std::string_view k;
    std::string_view v;
};

// Zero-allocation Request structure.
struct HttpReq {
    HttpMethod m;
    std::string_view p;
    std::vector<HttpHdr> hdrs;
    std::string_view b;
    char buf[4096]; // Pre-allocated slab for raw socket data
    size_t l = 0;

    void clear() { l = 0; hdrs.clear(); }
};

// Same for Response.
struct HttpRes {
    HttpStatus s;
    std::vector<HttpHdr> hdrs;
    std::string_view b;
    char buf[4096];
    size_t l = 0;

    void clear() { l = 0; hdrs.clear(); }
};

/**
 * Fast HTTP/1.1 Parser.
 * I developed this parser to be completely zero-allocation during the 
 * hot path. It just scans the buffer and maps views into the data.
 */
class HttpParser {
public:
    static HttpMethod s2m(std::string_view s) {
        if (s == "GET") return HttpMethod::GET;
        if (s == "POST") return HttpMethod::POST;
        if (s == "PUT") return HttpMethod::PUT;
        if (s == "DELETE") return HttpMethod::DEL; // Using DEL instead of DELETE
        return HttpMethod::UNK;
    }

    SNAP_HOT static bool parse_req(HttpReq& req) {
        std::string_view d(req.buf, req.l);
        size_t p = d.find("\r\n");
        if (p == std::string_view::npos) return false;

        // Line 1: METHOD PATH VERSION
        std::string_view line1 = d.substr(0, p);
        size_t m_end = line1.find(' ');
        if (m_end == std::string_view::npos) return false;
        req.m = s2m(line1.substr(0, m_end));
        
        size_t p_start = m_end + 1;
        size_t p_end = line1.find(' ', p_start);
        if (p_end == std::string_view::npos) return false;
        req.p = line1.substr(p_start, p_end - p_start);

        // Headers: scan for colon and map views
        size_t cur = p + 2;
        while (cur < d.size()) {
            size_t nxt = d.find("\r\n", cur);
            if (nxt == std::string_view::npos) break;
            if (nxt == cur) { // End of headers
                req.b = d.substr(nxt + 2);
                return true;
            }
            std::string_view h = d.substr(cur, nxt - cur);
            size_t sep = h.find(':');
            if (sep != std::string_view::npos) {
                req.hdrs.push_back({h.substr(0, sep), h.substr(sep + 2)});
            }
            cur = nxt + 2;
        }
        return true;
    }
};

} // namespace snap
