#pragma once

#include "internal.hpp"

#include <initializer_list>

// Minimal DER writer/reader (single-octet tags, definite lengths).
namespace compat {
namespace der {

enum Tag : uint8_t {
    BOOLEAN = 0x01,
    INTEGER = 0x02,
    BIT_STRING = 0x03,
    OCTET_STRING = 0x04,
    NULL_ = 0x05,
    OID = 0x06,
    UTF8String = 0x0C,
    PrintableString = 0x13,
    T61String = 0x14,
    IA5String = 0x16,
    UTCTime = 0x17,
    GeneralizedTime = 0x18,
    BMPString = 0x1E,
    SEQUENCE = 0x30,
    SET = 0x31,
};

inline constexpr uint8_t ctx(unsigned n, bool constructed = true)
{
    return uint8_t(0x80 | (constructed ? 0x20 : 0) | n);
}

Bytes tlv(uint8_t tag, const uint8_t *p, size_t n);
inline Bytes tlv(uint8_t tag, const Bytes &content) { return tlv(tag, content.data(), content.size()); }
Bytes seq(std::initializer_list<Bytes> parts);
Bytes set_of(std::vector<Bytes> parts); // sorted, as DER requires
Bytes concat(std::initializer_list<Bytes> parts);
void append(Bytes &out, const Bytes &part);

Bytes integer(long long v);
Bytes integer_unsigned(const uint8_t *p, size_t n);
inline Bytes integer_unsigned(const Bytes &b) { return integer_unsigned(b.data(), b.size()); }
Bytes oid(const Oid &o);
Bytes octet_string(const Bytes &b);
Bytes bit_string(const uint8_t *p, size_t nbits);
Bytes null();
Bytes boolean(bool v);
Bytes string(uint8_t tag, const std::string &s);
Bytes time(const std::string &t); // UTCTime (13 chars) or GeneralizedTime (15 chars)

struct Node {
    uint8_t tag = 0;
    const uint8_t *hdr = nullptr; // start of TLV
    const uint8_t *val = nullptr; // start of contents
    size_t len = 0;               // contents length
    size_t total = 0;             // header + contents
    bool constructed() const { return tag & 0x20; }
    Bytes raw() const { return Bytes(hdr, hdr + total); }
    Bytes value() const { return Bytes(val, val + len); }
};

class Reader {
public:
    Reader(const uint8_t *p, size_t n) : p_(p), end_(p + n) {}
    explicit Reader(const Bytes &b) : Reader(b.data(), b.size()) {}
    explicit Reader(Bytes &&) = delete; // nodes would point into a destroyed buffer
    explicit Reader(const Node &n) : Reader(n.val, n.len) {}
    bool empty() const { return p_ == end_; }
    bool peek(uint8_t tag) const { return p_ != end_ && *p_ == tag; }
    Node next();
    Node next(uint8_t tag);
    bool next_optional(uint8_t tag, Node &out);
    void expect_end() const;

private:
    const uint8_t *p_;
    const uint8_t *end_;
};

Node parse_one(const Bytes &b); // exactly one element, nothing trailing
Node parse_one(Bytes &&) = delete;
bool try_parse_tlv(const uint8_t *p, size_t n, Node &out);

long long get_integer(const Node &n);
Bytes get_unsigned(const Node &n); // INTEGER contents without sign octets
Oid get_oid(const Node &n);
Bytes get_bit_string(const Node &n, size_t &nbits);
std::string get_string(const Node &n); // any string type, converted to Latin-1 where possible
std::string get_time(const Node &n);
bool get_boolean(const Node &n);

} // namespace der
} // namespace compat
