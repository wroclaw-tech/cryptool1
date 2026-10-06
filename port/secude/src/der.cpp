#include "der.hpp"

#include <algorithm>

namespace compat {
namespace der {

namespace {

void put_length(Bytes &out, size_t n)
{
    if (n < 0x80) {
        out.push_back(uint8_t(n));
        return;
    }
    uint8_t buf[sizeof(size_t)];
    int k = 0;
    while (n) {
        buf[k++] = uint8_t(n & 0xff);
        n >>= 8;
    }
    out.push_back(uint8_t(0x80 | k));
    while (k)
        out.push_back(buf[--k]);
}

[[noreturn]] void bad(const char *what)
{
    fail(EDECODE, std::string("ASN.1 decoding failed: ") + what);
}

} // namespace

Bytes tlv(uint8_t tag, const uint8_t *p, size_t n)
{
    Bytes out;
    out.reserve(n + 6);
    out.push_back(tag);
    put_length(out, n);
    if (n)
        out.insert(out.end(), p, p + n);
    return out;
}

void append(Bytes &out, const Bytes &part)
{
    out.insert(out.end(), part.begin(), part.end());
}

Bytes concat(std::initializer_list<Bytes> parts)
{
    Bytes out;
    for (const auto &p : parts)
        append(out, p);
    return out;
}

Bytes seq(std::initializer_list<Bytes> parts)
{
    return tlv(SEQUENCE, concat(parts));
}

Bytes set_of(std::vector<Bytes> parts)
{
    std::sort(parts.begin(), parts.end());
    Bytes body;
    for (const auto &p : parts)
        append(body, p);
    return tlv(SET, body);
}

Bytes integer(long long v)
{
    Bytes b;
    unsigned long long u = static_cast<unsigned long long>(v);
    for (int i = 0; i < 8; ++i)
        b.insert(b.begin(), uint8_t(u >> (8 * i)));
    size_t i = 0;
    while (i + 1 < b.size() && ((b[i] == 0x00 && !(b[i + 1] & 0x80)) || (b[i] == 0xff && (b[i + 1] & 0x80))))
        ++i;
    return tlv(INTEGER, Bytes(b.begin() + long(i), b.end()));
}

Bytes integer_unsigned(const uint8_t *p, size_t n)
{
    while (n > 1 && *p == 0) {
        ++p;
        --n;
    }
    Bytes c;
    if (n == 0 || (p[0] & 0x80))
        c.push_back(0);
    c.insert(c.end(), p, p + n);
    return tlv(INTEGER, c);
}

Bytes oid(const Oid &o)
{
    if (o.size() < 2 || o[0] > 2)
        fail(EENCODE, "invalid object identifier");
    Bytes c;
    auto put = [&c](unsigned long long v) {
        uint8_t tmp[10];
        int k = 0;
        do {
            tmp[k++] = uint8_t(v & 0x7f);
            v >>= 7;
        } while (v);
        while (k > 1)
            c.push_back(uint8_t(tmp[--k] | 0x80));
        c.push_back(tmp[0]);
    };
    put(static_cast<unsigned long long>(o[0]) * 40 + o[1]);
    for (size_t i = 2; i < o.size(); ++i)
        put(o[i]);
    return tlv(OID, c);
}

Bytes octet_string(const Bytes &b) { return tlv(OCTET_STRING, b); }

Bytes bit_string(const uint8_t *p, size_t nbits)
{
    size_t n = (nbits + 7) / 8;
    Bytes c;
    c.push_back(uint8_t((8 - nbits % 8) % 8));
    if (n)
        c.insert(c.end(), p, p + n);
    if (nbits % 8)
        c.back() &= uint8_t(0xff << (8 - nbits % 8));
    return tlv(BIT_STRING, c);
}

Bytes null() { return Bytes{NULL_, 0}; }

Bytes boolean(bool v) { return Bytes{BOOLEAN, 1, uint8_t(v ? 0xff : 0)}; }

Bytes string(uint8_t tag, const std::string &s)
{
    if (tag == UTF8String) {
        std::string u;
        for (unsigned char ch : s) {
            if (ch < 0x80) {
                u += char(ch);
            } else {
                u += char(0xC0 | (ch >> 6));
                u += char(0x80 | (ch & 0x3f));
            }
        }
        return tlv(tag, reinterpret_cast<const uint8_t *>(u.data()), u.size());
    }
    if (tag == BMPString) {
        Bytes c;
        for (unsigned char ch : s) {
            c.push_back(0);
            c.push_back(ch);
        }
        return tlv(tag, c);
    }
    return tlv(tag, reinterpret_cast<const uint8_t *>(s.data()), s.size());
}

Bytes time(const std::string &t)
{
    uint8_t tag = t.size() >= 15 ? GeneralizedTime : UTCTime;
    return tlv(tag, reinterpret_cast<const uint8_t *>(t.data()), t.size());
}

bool try_parse_tlv(const uint8_t *p, size_t n, Node &out)
{
    if (n < 2)
        return false;
    const uint8_t *q = p;
    uint8_t tag = *q++;
    if ((tag & 0x1f) == 0x1f)
        return false;
    size_t len;
    uint8_t l0 = *q++;
    if (l0 < 0x80) {
        len = l0;
    } else {
        int k = l0 & 0x7f;
        if (k == 0 || k > 4 || size_t(k) > n - 2)
            return false;
        len = 0;
        for (int i = 0; i < k; ++i)
            len = (len << 8) | *q++;
    }
    size_t hdr = size_t(q - p);
    if (len > n - hdr)
        return false;
    out.tag = tag;
    out.hdr = p;
    out.val = q;
    out.len = len;
    out.total = hdr + len;
    return true;
}

Node Reader::next()
{
    Node n;
    if (!try_parse_tlv(p_, size_t(end_ - p_), n))
        bad("malformed element");
    p_ += n.total;
    return n;
}

Node Reader::next(uint8_t tag)
{
    Node n = next();
    if (n.tag != tag)
        bad("unexpected tag");
    return n;
}

bool Reader::next_optional(uint8_t tag, Node &out)
{
    if (!peek(tag))
        return false;
    out = next();
    return true;
}

void Reader::expect_end() const
{
    if (p_ != end_)
        bad("junk at end of element");
}

Node parse_one(const Bytes &b)
{
    Reader r(b);
    Node n = r.next();
    r.expect_end();
    return n;
}

long long get_integer(const Node &n)
{
    if (n.tag != INTEGER || n.len == 0 || n.len > 8)
        bad("INTEGER expected");
    long long v = (n.val[0] & 0x80) ? -1 : 0;
    for (size_t i = 0; i < n.len; ++i)
        v = static_cast<long long>((static_cast<unsigned long long>(v) << 8) | n.val[i]);
    return v;
}

Bytes get_unsigned(const Node &n)
{
    if (n.tag != INTEGER || n.len == 0)
        bad("INTEGER expected");
    return strip_leading_zeros(n.val, n.len);
}

Oid get_oid(const Node &n)
{
    if (n.tag != OID || n.len == 0)
        bad("OBJECT IDENTIFIER expected");
    Oid o;
    unsigned long long v = 0;
    bool first = true;
    for (size_t i = 0; i < n.len; ++i) {
        v = (v << 7) | (n.val[i] & 0x7f);
        if (v > 0xffffffffULL)
            bad("OBJECT IDENTIFIER component too large");
        if (!(n.val[i] & 0x80)) {
            if (first) {
                unsigned a = v < 40 ? 0 : v < 80 ? 1 : 2;
                o.push_back(a);
                o.push_back(unsigned(v - 40ULL * a));
                first = false;
            } else {
                o.push_back(unsigned(v));
            }
            v = 0;
        }
    }
    if (v != 0 || first)
        bad("truncated OBJECT IDENTIFIER");
    return o;
}

Bytes get_bit_string(const Node &n, size_t &nbits)
{
    if (n.tag != BIT_STRING || n.len == 0 || n.val[0] > 7)
        bad("BIT STRING expected");
    nbits = (n.len - 1) * 8 - n.val[0];
    return Bytes(n.val + 1, n.val + n.len);
}

std::string get_string(const Node &n)
{
    std::string s;
    if (n.tag == BMPString) {
        for (size_t i = 0; i + 1 < n.len; i += 2) {
            unsigned c = (unsigned(n.val[i]) << 8) | n.val[i + 1];
            s += c < 0x100 ? char(c) : '?';
        }
        return s;
    }
    if (n.tag == UTF8String) {
        for (size_t i = 0; i < n.len; ++i) {
            uint8_t c = n.val[i];
            if (c < 0x80) {
                s += char(c);
            } else if ((c & 0xE0) == 0xC0 && i + 1 < n.len) {
                unsigned cp = (unsigned(c & 0x1f) << 6) | (n.val[i + 1] & 0x3f);
                s += cp < 0x100 ? char(cp) : '?';
                ++i;
            } else {
                while (i + 1 < n.len && (n.val[i + 1] & 0xC0) == 0x80)
                    ++i;
                s += '?';
            }
        }
        return s;
    }
    if (n.constructed())
        bad("primitive string expected");
    return std::string(reinterpret_cast<const char *>(n.val), n.len);
}

std::string get_time(const Node &n)
{
    if (n.tag != UTCTime && n.tag != GeneralizedTime)
        bad("time expected");
    return std::string(reinterpret_cast<const char *>(n.val), n.len);
}

bool get_boolean(const Node &n)
{
    if (n.tag != BOOLEAN || n.len != 1)
        bad("BOOLEAN expected");
    return n.val[0] != 0;
}

} // namespace der

Bytes strip_leading_zeros(const uint8_t *p, size_t n)
{
    while (n > 1 && *p == 0) {
        ++p;
        --n;
    }
    return Bytes(p, p + n);
}

Bytes enc_unsigned_integer(const uint8_t *p, size_t n)
{
    return der::integer_unsigned(p, n);
}

} // namespace compat
