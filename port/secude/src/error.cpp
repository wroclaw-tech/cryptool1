#include "internal.hpp"

#include <cstdarg>
#include <deque>

namespace compat {

namespace {

struct Entry {
    int number;
    std::string text;
    std::string proc;
};

constexpr size_t kMaxErrors = 32;

std::deque<Entry> &stack()
{
    thread_local std::deque<Entry> s;
    return s;
}

struct NamedError {
    int code;
    const char *name;
    const char *text;
};

const NamedError kErrors[] = {
    {EALGID, "EALGID", "invalid algorithm identifier"},
    {EOBJ, "EOBJ", "PSE object not found"},
    {EKEYSIZE, "EKEYSIZE", "invalid key size"},
    {EVERIFY, "EVERIFY", "signature verification failed"},
    {ESYSTEM, "ESYSTEM", "system error"},
    {EMALLOC, "EMALLOC", "memory allocation failed"},
    {EENCODE, "EENCODE", "ASN.1 encoding failed"},
    {EDECODE, "EDECODE", "ASN.1 decoding failed"},
    {EREADPSE, "EREADPSE", "cannot read PSE"},
    {EWRITEPSE, "EWRITEPSE", "cannot write PSE"},
    {EROOTCERT, "EROOTCERT", "root certificate missing"},
    {ENODIR, "ENODIR", "no CA directory"},
    {EWRITEFILE, "EWRITEFILE", "cannot write file"},
    {EREADFILE, "EREADFILE", "cannot read file"},
    {EINVALID, "EINVALID", "invalid parameter"},
    {EFILENOTEXISTING, "EFILENOTEXISTING", "file does not exist"},
    {EWRONGPARM, "EWRONGPARM", "wrong parameter"},
    {EINTERNAL, "EINTERNAL", "internal error"},
    {ENOTSUPPORTED, "ENOTSUPPORTED", "function not supported"},
    {EUNKNOWNALGID, "EUNKNOWNALGID", "unknown algorithm"},
    {EALGNOTFITKEY, "EALGNOTFITKEY", "algorithm does not fit the key"},
    {EDECRYPTION, "EDECRYPTION", "decryption failed"},
    {EPIN, "EPIN", "wrong PIN"},
    {EPSEALREADYEXISTING, "EPSEALREADYEXISTING", "PSE already exists"},
    {EPSENOTEXISTING, "EPSENOTEXISTING", "PSE does not exist"},
    {EPSEOBJECTNOTEXISTING, "EPSEOBJECTNOTEXISTING", "PSE object does not exist"},
    {EPSEFILE, "EPSEFILE", "not a valid PSE file"},
    {EPSEFILESTRUCTURE, "EPSEFILESTRUCTURE", "invalid PSE file structure"},
    {ECADBCERT, "ECADBCERT", "certificate not found in CA database"},
    {ECADBUSER, "ECADBUSER", "user not found in CA database"},
    {ENOCRL, "ENOCRL", "no revocation list available"},
    {ENONAME, "ENONAME", "no name available"},
    {EVERIFICATION, "EVERIFICATION", "verification failed"},
    {ERANDOM, "ERANDOM", "random number generation failed"},
};

} // namespace

const char *error_name(int code)
{
    for (const auto &e : kErrors)
        if (e.code == code)
            return e.name;
    return "ERROR";
}

static const char *error_text(int code)
{
    for (const auto &e : kErrors)
        if (e.code == code)
            return e.text;
    return "error";
}

void fail(int code, const std::string &text)
{
    throw Error(code, text.empty() ? error_text(code) : text);
}

void push_error(int code, const char *proc, const std::string &text)
{
    auto &s = stack();
    if (s.size() >= kMaxErrors)
        s.pop_front();
    s.push_back(Entry{code, text.empty() ? error_text(code) : text, proc ? proc : ""});
}

std::string str_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = std::vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    std::string out;
    if (n > 0) {
        out.resize(size_t(n) + 1);
        std::vsnprintf(&out[0], out.size(), fmt, ap2);
        out.resize(size_t(n));
    }
    va_end(ap2);
    return out;
}

char *append_string(char *string, const std::string &text)
{
    if (!string)
        return mem_strdup(text);
    size_t old = std::strlen(string);
    char *r = static_cast<char *>(mem_realloc(string, old + text.size() + 1));
    std::memcpy(r + old, text.data(), text.size());
    r[old + text.size()] = 0;
    return r;
}

} // namespace compat

using namespace compat;

extern "C" {

int th_last_error(void)
{
    auto &s = stack();
    return s.empty() ? 0 : s.back().number;
}

char *th_get_last_error_text(void)
{
    auto &s = stack();
    static char empty[1] = {0};
    return s.empty() ? empty : const_cast<char *>(s.back().text.c_str());
}

struct ErrStack *th_remove_last_error(void)
{
    return guarded<struct ErrStack *>("th_remove_last_error", nullptr, [] {
        auto &s = stack();
        ErrStack *e = mem_new<ErrStack>();
        e->e_is_error = TRUE;
        e->e_addrtype = int_n;
        if (!s.empty()) {
            e->e_number = s.back().number;
            e->e_text = mem_strdup(s.back().text);
            e->e_proc = mem_strdup(s.back().proc);
            s.pop_back();
        }
        return e;
    });
}

void global_add_error(int number, char *text, char *addr, Struct_No addrtype, char *proc)
{
    push_error(number, proc, text ? text : "");
}

void aux_free_error(void)
{
    stack().clear();
}

char *aux_sprint_error(PSE pse_handle, char *string, int verbose)
{
    return guarded<char *>("aux_sprint_error", nullptr, [&] {
        auto &s = stack();
        std::string out;
        if (s.empty())
            out = "no error\n";
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            out += str_printf("%s (0x%04X): %s", error_name(it->number), unsigned(it->number), it->text.c_str());
            if (verbose && !it->proc.empty())
                out += " [" + it->proc + "]";
            out += "\n";
            if (!verbose)
                break;
        }
        return append_string(string, out);
    });
}

} // extern "C"
