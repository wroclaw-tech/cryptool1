// Text dumps: ASN.1 structure dump (sdumpasn) and hex dumps (aux_sxdump).
#include "der.hpp"

#include <algorithm>
#include <cstdio>

namespace compat {

namespace {

const char *tag_name(uint8_t tag)
{
    switch (tag & 0x1f) {
    case 0x01: return "BOOLEAN";
    case 0x02: return "INTEGER";
    case 0x03: return "BIT STRING";
    case 0x04: return "OCTET STRING";
    case 0x05: return "NULL";
    case 0x06: return "OBJECT IDENTIFIER";
    case 0x0A: return "ENUMERATED";
    case 0x0C: return "UTF8String";
    case 0x10: return "SEQUENCE";
    case 0x11: return "SET";
    case 0x12: return "NumericString";
    case 0x13: return "PrintableString";
    case 0x14: return "T61String";
    case 0x16: return "IA5String";
    case 0x17: return "UTCTime";
    case 0x18: return "GeneralizedTime";
    case 0x1A: return "VisibleString";
    case 0x1E: return "BMPString";
    default: return nullptr;
    }
}

struct Ber {
    uint8_t tag;
    size_t hdr;
    size_t len;
    bool indefinite;
};

// Parses one BER header at p; returns false if malformed.
bool ber_header(const uint8_t *p, size_t n, Ber &b)
{
    if (n < 2 || (p[0] & 0x1f) == 0x1f)
        return false;
    b.tag = p[0];
    b.indefinite = false;
    if (p[1] < 0x80) {
        b.len = p[1];
        b.hdr = 2;
    } else if (p[1] == 0x80) {
        if (!(b.tag & 0x20))
            return false;
        b.indefinite = true;
        b.len = 0;
        b.hdr = 2;
    } else {
        size_t k = p[1] & 0x7f;
        if (k > 4 || n < 2 + k)
            return false;
        b.len = 0;
        for (size_t i = 0; i < k; ++i)
            b.len = (b.len << 8) | p[2 + i];
        b.hdr = 2 + k;
    }
    return b.indefinite || b.len <= n - b.hdr;
}

class Dumper {
public:
    Dumper(int options, int base) : options_(options), base_(base) {}

    // Dumps all elements in [p, p+n); returns consumed length or SIZE_MAX on error.
    size_t elements(const uint8_t *p, size_t n, size_t offset, int depth, bool until_eoc)
    {
        size_t pos = 0;
        while (pos < n) {
            if (until_eoc && n - pos >= 2 && p[pos] == 0 && p[pos + 1] == 0)
                return pos + 2;
            size_t used = element(p + pos, n - pos, offset + pos, depth);
            if (used == SIZE_MAX)
                return SIZE_MAX;
            pos += used;
        }
        return until_eoc ? SIZE_MAX : pos;
    }

    size_t element(const uint8_t *p, size_t n, size_t offset, int depth)
    {
        Ber b;
        if (!ber_header(p, n, b))
            return SIZE_MAX;
        std::string indent(size_t(depth) * 2, ' ');
        std::string head = str_printf("%6lu %4s: ", static_cast<unsigned long>(offset + size_t(base_)),
                                      b.indefinite ? "NDEF" : std::to_string(b.len).c_str());
        std::string name = describe_tag(b.tag);
        const uint8_t *v = p + b.hdr;
        if (b.tag & 0x20) {
            out_ += head + indent + name + " {\n";
            size_t used = b.indefinite ? elements(v, n - b.hdr, offset + b.hdr, depth + 1, true)
                                       : elements(v, b.len, offset + b.hdr, depth + 1, false);
            if (used == SIZE_MAX)
                return SIZE_MAX;
            out_ += std::string(6 + 1 + 4 + 2, ' ') + indent + "}\n";
            return b.hdr + (b.indefinite ? used : b.len);
        }
        out_ += head + indent + name + primitive(b.tag, v, b.len, offset + b.hdr, depth);
        return b.hdr + b.len;
    }

    std::string text() const { return out_; }
    std::string &text() { return out_; }

private:
    std::string describe_tag(uint8_t tag) const
    {
        uint8_t cls = tag & 0xc0;
        if (cls == 0) {
            const char *n = tag_name(tag);
            return n ? n : str_printf("[UNIVERSAL %u]", tag & 0x1f);
        }
        if (cls == 0x80)
            return str_printf("[%u]", tag & 0x1f);
        return str_printf("[%s %u]", cls == 0x40 ? "APPLICATION" : "PRIVATE", tag & 0x1f);
    }

    bool encapsulated(const uint8_t *v, size_t n) const
    {
        if (n < 2 || (v[0] != 0x30 && v[0] != 0x31))
            return false;
        size_t pos = 0;
        while (pos < n) {
            Ber b;
            if (!ber_header(v + pos, n - pos, b) || b.indefinite)
                return false;
            pos += b.hdr + b.len;
        }
        return pos == n;
    }

    std::string hexlines(const uint8_t *v, size_t n, int depth) const
    {
        size_t limit = (options_ & ASN1dump_no_length_limit) ? n : std::min<size_t>(n, 128);
        std::string indent(size_t(6 + 1 + 4 + 2 + 2 * depth + 2), ' ');
        std::string s = hexdump(v, limit, indent, false);
        if (limit < n)
            s += indent + str_printf("[ Another %lu bytes skipped ]\n", static_cast<unsigned long>(n - limit));
        return s;
    }

    std::string primitive(uint8_t tag, const uint8_t *v, size_t n, size_t offset, int depth)
    {
        if (tag & 0xc0)
            return "\n" + hexlines(v, n, depth);
        try {
            Bytes tlv = der::tlv(tag, v, n);
            der::Node node = der::parse_one(tlv);
            switch (tag) {
            case 0x01:
                return std::string(" ") + (der::get_boolean(node) ? "TRUE" : "FALSE") + "\n";
            case 0x02:
            case 0x0A:
                if (n <= 4)
                    return " " + std::to_string(der::get_integer(node)) + "\n";
                return "\n" + hexlines(v, n, depth);
            case 0x05:
                return "\n";
            case 0x06: {
                Oid o = der::get_oid(node);
                const AlgInfo *a = alg_by_oid(o);
                return " " + oid_to_string(o) + (a ? std::string(" (") + a->name + ")" : "") + "\n";
            }
            case 0x03:
            case 0x04: {
                const uint8_t *data = tag == 0x03 ? v + 1 : v;
                size_t len = tag == 0x03 ? (n ? n - 1 : 0) : n;
                if (tag == 0x03 && (n == 0 || v[0] != 0))
                    return "\n" + hexlines(v, n, depth);
                if (encapsulated(data, len)) {
                    Dumper inner(options_, base_);
                    inner.elements(data, len, offset + (tag == 0x03 ? 1 : 0), depth + 1, false);
                    return ", encapsulates {\n" + inner.text() + std::string(6 + 1 + 4 + 2, ' ') +
                           std::string(size_t(depth) * 2, ' ') + "}\n";
                }
                return "\n" + hexlines(v, n, depth);
            }
            case 0x17:
            case 0x18:
                return " " + format_time(der::get_time(node).c_str()) + "\n";
            default:
                if (tag_name(tag))
                    return " '" + der::get_string(node) + "'\n";
                return "\n" + hexlines(v, n, depth);
            }
        } catch (const Error &) {
            return "\n" + hexlines(v, n, depth);
        }
    }

    int options_;
    int base_;
    std::string out_;
};

std::string dump_asn1(const uint8_t *p, size_t n, int options, int base)
{
    Dumper d(options, base);
    if (n == 0)
        return "FATAL: decoding failed! (empty input)\n";
    if (options & ASN1dump_multiple_PDU) {
        if (d.elements(p, n, 0, 0, false) == SIZE_MAX)
            d.text() += "FATAL: decoding failed!\n";
        return d.text();
    }
    size_t used = d.element(p, n, 0, 0);
    if (used == SIZE_MAX) {
        d.text() += "FATAL: decoding failed!\n";
        return d.text();
    }
    if (used < n)
        d.text() += str_printf("junk at end of PDU (%lu octets)\n", static_cast<unsigned long>(n - used));
    return d.text();
}

} // namespace

std::string hexdump(const uint8_t *p, size_t n, const std::string &indent, bool ascii)
{
    std::string s;
    for (size_t off = 0; off < n; off += 16) {
        std::string line = indent + str_printf("%6lX  ", static_cast<unsigned long>(off));
        std::string text;
        for (size_t i = 0; i < 16; ++i) {
            if (i && i % 4 == 0)
                line += ' ';
            if (i == 8)
                line += ' ';
            if (off + i < n) {
                uint8_t c = p[off + i];
                line += str_printf("%02X", c);
                text += (c >= 0x20 && c < 0x7f) ? char(c) : '.';
            } else if (ascii) {
                line += "  ";
            }
        }
        if (ascii)
            line += "  " + text;
        while (!line.empty() && line.back() == ' ')
            line.pop_back();
        s += line + "\n";
    }
    return s;
}

} // namespace compat

using namespace compat;

extern "C" {

char *sdumpasn(OctetString *ostr, int options, int displayed_offset)
{
    return guarded<char *>("sdumpasn", nullptr, [&] {
        Bytes b = bytes_of(ostr);
        return mem_strdup(dump_asn1(b.data(), b.size(), options, displayed_offset));
    });
}

void aux_adump(char *from, int num, int options, int displayed_offset)
{
    guarded_void("aux_adump", [&] {
        if (!from || num < 0)
            fail(EINVALID, "missing input");
        std::fputs(dump_asn1(reinterpret_cast<const uint8_t *>(from), size_t(num), options, displayed_offset).c_str(), stdout);
    });
}

char *aux_sxdump(char *string, char *buffer, sec_int4 len, sec_int4 addr_type)
{
    return guarded<char *>("aux_sxdump", nullptr, [&] {
        if (!buffer && len > 0)
            fail(EINVALID, "missing buffer");
        return append_string(string, hexdump(reinterpret_cast<const uint8_t *>(buffer), len > 0 ? size_t(len) : 0, "", true));
    });
}

char *aux_sxdump2(char *string, char *buffer, sec_int4 len, sec_int4 addr_type)
{
    return guarded<char *>("aux_sxdump2", nullptr, [&] {
        if (!buffer && len > 0)
            fail(EINVALID, "missing buffer");
        return append_string(string, hexdump(reinterpret_cast<const uint8_t *>(buffer), len > 0 ? size_t(len) : 0, "", false));
    });
}

} // extern "C"
