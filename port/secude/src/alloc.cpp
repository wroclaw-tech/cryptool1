#include "internal.hpp"

#include <mutex>
#include <unordered_set>

namespace compat {

namespace {

struct Registry {
    std::mutex lock;
    std::unordered_set<void *> live;
};

Registry &registry()
{
    static Registry *r = new Registry;
    return *r;
}

} // namespace

void *mem_alloc(size_t n)
{
    void *p = std::calloc(1, n ? n : 1);
    if (!p)
        throw std::bad_alloc();
    Registry &r = registry();
    std::lock_guard<std::mutex> g(r.lock);
    r.live.insert(p);
    return p;
}

void *mem_realloc(void *p, size_t n)
{
    if (!p)
        return mem_alloc(n);
    Registry &r = registry();
    std::lock_guard<std::mutex> g(r.lock);
    if (!r.live.count(p))
        throw Error(EINVALID, "realloc of foreign memory");
    void *q = std::realloc(p, n ? n : 1);
    if (!q)
        throw std::bad_alloc();
    r.live.erase(p);
    r.live.insert(q);
    return q;
}

void mem_free(void *p)
{
    if (!p)
        return;
    Registry &r = registry();
    {
        std::lock_guard<std::mutex> g(r.lock);
        if (!r.live.erase(p))
            return;
    }
    std::free(p);
}

bool mem_owned(const void *p)
{
    if (!p)
        return false;
    Registry &r = registry();
    std::lock_guard<std::mutex> g(r.lock);
    return r.live.count(const_cast<void *>(p)) != 0;
}

char *mem_strdup(const char *s)
{
    if (!s)
        return nullptr;
    size_t n = std::strlen(s);
    char *d = static_cast<char *>(mem_alloc(n + 1));
    std::memcpy(d, s, n);
    return d;
}

char *mem_strdup(const std::string &s)
{
    char *d = static_cast<char *>(mem_alloc(s.size() + 1));
    std::memcpy(d, s.data(), s.size());
    return d;
}

void fill_ostr(OctetString *o, const uint8_t *data, size_t len)
{
    char *buf = static_cast<char *>(mem_alloc(len + 1));
    if (len)
        std::memcpy(buf, data, len);
    o->octets = buf;
    o->noctets = static_cast<sec_uint4>(len);
}

OctetString *new_ostr(const uint8_t *data, size_t len)
{
    OctetString *o = mem_new<OctetString>();
    fill_ostr(o, data, len);
    return o;
}

void fill_bstr(BitString *o, const Bytes &b, size_t nbits)
{
    char *buf = static_cast<char *>(mem_alloc(b.size() + 1));
    if (!b.empty())
        std::memcpy(buf, b.data(), b.size());
    o->bits = buf;
    o->nbits = static_cast<sec_uint4>(nbits);
}

BitString *new_bstr(const Bytes &b, size_t nbits)
{
    BitString *o = mem_new<BitString>();
    fill_bstr(o, b, nbits);
    return o;
}

Bytes bytes_of(const OctetString *o)
{
    if (!o || !o->octets || !o->noctets)
        return {};
    const uint8_t *p = reinterpret_cast<const uint8_t *>(o->octets);
    return Bytes(p, p + o->noctets);
}

Bytes bytes_of(const BitString *b)
{
    if (!b || !b->bits || !b->nbits)
        return {};
    const uint8_t *p = reinterpret_cast<const uint8_t *>(b->bits);
    return Bytes(p, p + (b->nbits + 7) / 8);
}

Bytes bytes_of(const char *s)
{
    if (!s)
        return {};
    return Bytes(s, s + std::strlen(s));
}

} // namespace compat

using namespace compat;

extern "C" {

void *aux_malloc(size_t size)
{
    return guarded<void *>("aux_malloc", nullptr, [&] { return mem_alloc(size); });
}

void *aux_calloc(size_t nr, size_t size)
{
    return guarded<void *>("aux_calloc", nullptr, [&]() -> void * {
        if (size && nr > SIZE_MAX / size)
            throw std::bad_alloc();
        return mem_alloc(nr * size);
    });
}

void aux_free(void *ptr)
{
    mem_free(ptr);
}

char *aux_cpy_String(char *str)
{
    return guarded<char *>("aux_cpy_String", nullptr, [&] { return mem_strdup(str); });
}

void aux_free_String(char **str)
{
    if (!str)
        return;
    mem_free(*str);
    *str = nullptr;
}

OctetString *aux_alloc_OctetString(void)
{
    return guarded<OctetString *>("aux_alloc_OctetString", nullptr, [] { return mem_new<OctetString>(); });
}

OctetString *aux_cpy_OctetString(OctetString *ostr)
{
    return guarded<OctetString *>("aux_cpy_OctetString", nullptr, [&]() -> OctetString * {
        if (!ostr)
            return nullptr;
        return new_ostr(bytes_of(ostr));
    });
}

void aux_free2_OctetString(OctetString *ostr)
{
    if (!ostr)
        return;
    mem_free(ostr->octets);
    ostr->octets = nullptr;
    ostr->noctets = 0;
}

void aux_free_OctetString(OctetString **ostr)
{
    if (!ostr || !*ostr)
        return;
    if (!mem_owned(*ostr)) {
        *ostr = nullptr;
        return;
    }
    aux_free2_OctetString(*ostr);
    mem_free(*ostr);
    *ostr = nullptr;
}

int aux_cpy2_BitString(BitString *dup_bstr, BitString *bstr)
{
    return guarded<int>("aux_cpy2_BitString", -1, [&] {
        if (!dup_bstr || !bstr)
            fail(EINVALID, "missing parameter");
        fill_bstr(dup_bstr, bytes_of(bstr), bstr->nbits);
        return 0;
    });
}

void aux_free2_BitString(BitString *bstr)
{
    if (!bstr)
        return;
    mem_free(bstr->bits);
    bstr->bits = nullptr;
    bstr->nbits = 0;
}

void aux_free_BitString(BitString **bstr)
{
    if (!bstr || !*bstr)
        return;
    if (!mem_owned(*bstr)) {
        *bstr = nullptr;
        return;
    }
    aux_free2_BitString(*bstr);
    mem_free(*bstr);
    *bstr = nullptr;
}

OctetString *aux_BString2OString(BitString *bstr)
{
    return guarded<OctetString *>("aux_BString2OString", nullptr, [&]() -> OctetString * {
        if (!bstr)
            fail(EINVALID, "missing BitString");
        return new_ostr(bytes_of(bstr));
    });
}

BitString *aux_OString2BString(OctetString *ostr)
{
    return guarded<BitString *>("aux_OString2BString", nullptr, [&]() -> BitString * {
        if (!ostr)
            fail(EINVALID, "missing OctetString");
        return new_bstr(bytes_of(ostr), size_t(ostr->noctets) * 8);
    });
}

} // extern "C"
