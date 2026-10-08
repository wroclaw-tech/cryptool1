// Distinguished names: string form "CN=..., DC=..., DC=..." (least significant
// RDN first, as in RFC 4514) <-> DName list in DER order.
#include "der.hpp"

#include <algorithm>
#include <cctype>
#include <strings.h>

namespace compat {

namespace {

struct AttrKeyword {
    const char *keyword;
    Oid oid;
    uint8_t tag;
};

const std::vector<AttrKeyword> &keywords()
{
    static const std::vector<AttrKeyword> k = {
        {"CN", {2, 5, 4, 3}, der::PrintableString},
        {"C", {2, 5, 4, 6}, der::PrintableString},
        {"L", {2, 5, 4, 7}, der::PrintableString},
        {"ST", {2, 5, 4, 8}, der::PrintableString},
        {"SP", {2, 5, 4, 8}, der::PrintableString},
        {"STREET", {2, 5, 4, 9}, der::PrintableString},
        {"O", {2, 5, 4, 10}, der::PrintableString},
        {"OU", {2, 5, 4, 11}, der::PrintableString},
        {"T", {2, 5, 4, 12}, der::PrintableString},
        {"TITLE", {2, 5, 4, 12}, der::PrintableString},
        {"SN", {2, 5, 4, 4}, der::PrintableString},
        {"SERIALNUMBER", {2, 5, 4, 5}, der::PrintableString},
        {"G", {2, 5, 4, 42}, der::PrintableString},
        {"GN", {2, 5, 4, 42}, der::PrintableString},
        {"DC", {0, 9, 2342, 19200300, 100, 1, 25}, der::IA5String},
        {"UID", {0, 9, 2342, 19200300, 100, 1, 1}, der::UTF8String},
        {"EMAIL", {1, 2, 840, 113549, 1, 9, 1}, der::IA5String},
        {"E", {1, 2, 840, 113549, 1, 9, 1}, der::IA5String},
    };
    return k;
}

const AttrKeyword *keyword_by_name(const std::string &name)
{
    for (const auto &k : keywords())
        if (!strcasecmp(k.keyword, name.c_str()))
            return &k;
    return nullptr;
}

const AttrKeyword *keyword_by_oid(const Oid &o)
{
    for (const auto &k : keywords())
        if (k.oid == o)
            return &k;
    return nullptr;
}

bool printable(const std::string &s)
{
    for (unsigned char c : s)
        if (!(std::isalnum(c) || std::strchr(" '()+,-./:=?", c)) || c >= 0x80)
            return false;
    return true;
}

std::string trim(const std::string &s)
{
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos)
        return {};
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

Oid parse_dotted(const std::string &s)
{
    Oid o;
    size_t i = 0;
    while (i < s.size()) {
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            return {};
        unsigned long v = std::strtoul(s.c_str() + i, nullptr, 10);
        o.push_back(unsigned(v));
        size_t dot = s.find('.', i);
        if (dot == std::string::npos)
            break;
        i = dot + 1;
    }
    return o.size() >= 2 ? o : Oid{};
}

std::string value_string(const AttrValueAssertion *ava)
{
    Bytes v = bytes_of(ava->element_IF_1);
    uint8_t tag = uint8_t(ava->attr_encoding);
    Bytes tlv = der::tlv(tag, v);
    der::Node n;
    der::try_parse_tlv(tlv.data(), tlv.size(), n);
    if (tag == der::UTF8String || tag == der::BMPString)
        return der::get_string(n);
    return std::string(v.begin(), v.end());
}

std::string normalize(const std::string &s)
{
    std::string out;
    bool space = false;
    for (unsigned char c : trim(s)) {
        if (c == ' ' || c == '\t') {
            space = true;
            continue;
        }
        if (space)
            out += ' ';
        space = false;
        out += char(std::tolower(c));
    }
    return out;
}

struct ParsedAva {
    Oid oid;
    uint8_t tag;
    Bytes value;
};

// Splits at unquoted/unescaped separators.
std::vector<std::string> split(const std::string &s, const char *seps)
{
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size()) {
            cur += c;
            cur += s[++i];
            continue;
        }
        if (c == '"')
            quoted = !quoted;
        if (!quoted && std::strchr(seps, c)) {
            out.push_back(cur);
            cur.clear();
            continue;
        }
        cur += c;
    }
    out.push_back(cur);
    return out;
}

std::string unescape(const std::string &raw)
{
    std::string v = trim(raw);
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
        v = v.substr(1, v.size() - 2);
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == '\\' && i + 1 < v.size())
            ++i;
        out += v[i];
    }
    return out;
}

ParsedAva parse_ava(const std::string &text)
{
    size_t eq = text.find('=');
    if (eq == std::string::npos)
        fail(EWRONGNAME, "invalid name component '" + trim(text) + "'");
    std::string key = trim(text.substr(0, eq));
    std::string value = unescape(text.substr(eq + 1));
    ParsedAva a;
    if (const AttrKeyword *k = keyword_by_name(key)) {
        a.oid = k->oid;
        a.tag = k->tag;
    } else {
        a.oid = parse_dotted(key);
        if (a.oid.empty())
            fail(EWRONGNAME, "unknown attribute type '" + key + "'");
        a.tag = der::PrintableString;
    }
    if (a.tag == der::PrintableString && !printable(value))
        a.tag = der::UTF8String;
    Bytes tlv = der::string(a.tag, value);
    a.value = der::parse_one(tlv).value();
    return a;
}

} // namespace

DName *string_to_dname(const char *s)
{
    if (!s)
        fail(EWRONGNAME, "missing name");
    std::vector<std::vector<ParsedAva>> rdns;
    for (const std::string &rdn : split(s, ",;")) {
        if (trim(rdn).empty())
            continue;
        std::vector<ParsedAva> avas;
        for (const std::string &ava : split(rdn, "+"))
            avas.push_back(parse_ava(ava));
        rdns.push_back(std::move(avas));
    }
    if (rdns.empty())
        fail(EWRONGNAME, "empty name");
    std::reverse(rdns.begin(), rdns.end());
    DName *head = nullptr, **tail = &head;
    for (const auto &avas : rdns) {
        DName *d = mem_new<DName>();
        *tail = d;
        tail = &d->next;
        RDName **rtail = &d->element_IF_2;
        for (const auto &a : avas) {
            RDName *r = mem_new<RDName>();
            *rtail = r;
            rtail = &r->next;
            r->member_IF_0 = mem_new<AttrValueAssertion>();
            r->member_IF_0->element_IF_0 = new_objid(a.oid);
            r->member_IF_0->element_IF_1 = new_ostr(a.value);
            r->member_IF_0->attr_encoding = static_cast<SEC_attr_encoding>(a.tag);
        }
    }
    return head;
}

static std::string ava_to_string(const AttrValueAssertion *ava)
{
    Oid o = oid_of(ava->element_IF_0);
    const AttrKeyword *k = keyword_by_oid(o);
    std::string key = k ? k->keyword : oid_to_string(o);
    std::string v = value_string(ava);
    bool quote = v.find_first_of(",;+\"") != std::string::npos || (!v.empty() && (v.front() == ' ' || v.back() == ' '));
    if (quote) {
        std::string q = "\"";
        for (char c : v) {
            if (c == '"' || c == '\\')
                q += '\\';
            q += c;
        }
        v = q + "\"";
    }
    return key + "=" + v;
}

std::string dname_to_string(const DName *d)
{
    std::vector<std::string> rdns;
    for (; d; d = d->next) {
        std::string r;
        for (const RDName *rd = d->element_IF_2; rd; rd = rd->next) {
            if (!rd->member_IF_0)
                continue;
            if (!r.empty())
                r += " + ";
            r += ava_to_string(rd->member_IF_0);
        }
        rdns.push_back(r);
    }
    std::string out;
    for (auto it = rdns.rbegin(); it != rdns.rend(); ++it) {
        if (!out.empty())
            out += ", ";
        out += *it;
    }
    return out;
}

bool dname_equal(const DName *a, const DName *b)
{
    for (; a && b; a = a->next, b = b->next) {
        const RDName *x = a->element_IF_2, *y = b->element_IF_2;
        for (; x && y; x = x->next, y = y->next) {
            if (!x->member_IF_0 || !y->member_IF_0)
                return false;
            if (!oid_equal(x->member_IF_0->element_IF_0, y->member_IF_0->element_IF_0))
                return false;
            if (normalize(value_string(x->member_IF_0)) != normalize(value_string(y->member_IF_0)))
                return false;
        }
        if (x || y)
            return false;
    }
    return !a && !b;
}

} // namespace compat

using namespace compat;

extern "C" {

DName *aux_Name2DName(char *name)
{
    return guarded<DName *>("aux_Name2DName", nullptr, [&] { return string_to_dname(name); });
}

char *aux_DName2Name(DName *name_ae)
{
    return guarded<char *>("aux_DName2Name", nullptr, [&]() -> char * {
        if (!name_ae)
            fail(ENONAME, "missing name");
        return mem_strdup(dname_to_string(name_ae));
    });
}

char *aux_DName2Attr(DName *dname, char *attr_key)
{
    return guarded<char *>("aux_DName2Attr", nullptr, [&]() -> char * {
        if (!attr_key)
            fail(EINVALID, "missing attribute keyword");
        Oid wanted;
        if (const AttrKeyword *k = keyword_by_name(attr_key))
            wanted = k->oid;
        else
            wanted = parse_dotted(attr_key);
        for (const DName *d = dname; d; d = d->next)
            for (const RDName *r = d->element_IF_2; r; r = r->next)
                if (r->member_IF_0 && oid_equal(r->member_IF_0->element_IF_0, wanted))
                    return mem_strdup(value_string(r->member_IF_0));
        fail(ENONAME, std::string("attribute ") + attr_key + " not present");
    });
}

int aux_cmp_DName(DName *a, DName *b)
{
    return dname_equal(a, b) ? 0 : 1;
}

} // extern "C"
