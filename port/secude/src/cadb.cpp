// CA database: <cadir>/cadb.der
//   CADatabase ::= SEQUENCE { version INTEGER (1), entries SEQUENCE OF Entry }
//   Entry      ::= SEQUENCE { keyType INTEGER, issued Time, certificate Certificate }
#include "der.hpp"

#include <algorithm>

namespace compat {

namespace {

const char kFileName[] = "cadb.der";

struct Entry {
    int key_type;
    std::string issued;
    Bytes cert;
};

std::string db_path(PSE pse)
{
    const char *dir = pse_cadir(pse);
    if (!dir || !*dir)
        fail(ENODIR, "PSE was opened without CA directory");
    std::string p = dir;
    if (p.back() != '/' && p.back() != '\\')
        p += '/';
    return p + kFileName;
}

std::vector<Entry> load(const std::string &path)
{
    bool exists = false;
    Bytes file = read_file(path, exists);
    std::vector<Entry> out;
    if (!exists)
        return out;
    der::Reader top(der::parse_one(file));
    if (der::get_integer(top.next(der::INTEGER)) != 1)
        fail(EFILECORRUPTED, "unsupported CA database version");
    der::Reader entries(top.next(der::SEQUENCE));
    while (!entries.empty()) {
        der::Reader e(entries.next(der::SEQUENCE));
        Entry x;
        x.key_type = int(der::get_integer(e.next(der::INTEGER)));
        x.issued = der::get_time(e.next());
        x.cert = e.next(der::SEQUENCE).raw();
        out.push_back(std::move(x));
    }
    return out;
}

void save(const std::string &path, const std::vector<Entry> &entries)
{
    Bytes body;
    for (const auto &e : entries)
        der::append(body, der::seq({der::integer(e.key_type), der::time(e.issued), e.cert}));
    write_file_atomic(path, der::seq({der::integer(1), der::tlv(der::SEQUENCE, body)}), 0644);
}

Bytes serial_of(const Certificate *c)
{
    Bytes s = bytes_of(c->tbs->serialnumber);
    return strip_leading_zeros(s.data(), s.size());
}

struct CertHolder {
    Certificate *c;
    explicit CertHolder(Certificate *cert) : c(cert) {}
    ~CertHolder() { free_certificate(c); }
    CertHolder(const CertHolder &) = delete;
    CertHolder &operator=(const CertHolder &) = delete;
};

} // namespace

} // namespace compat

using namespace compat;

extern "C" {

int af_cadb_add_Certificate(PSE pse_handle, KeyType keytype, Certificate *newcert)
{
    return guarded<int>("af_cadb_add_Certificate", -1, [&] {
        if (!newcert || !newcert->tbs)
            fail(EINVALID, "missing certificate");
        std::string path = db_path(pse_handle);
        std::vector<Entry> entries = load(path);
        Bytes serial = serial_of(newcert);
        for (const auto &e : entries) {
            CertHolder c(dec_certificate(e.cert));
            if (serial_of(c.c) == serial)
                fail(ECADBCERT, "a certificate with this serial number is already registered");
        }
        entries.push_back(Entry{int(keytype), utc_now(), enc_certificate(newcert)});
        save(path, entries);
        return 0;
    });
}

Certificate *af_cadb_get_Certificate(PSE pse_handle, OctetString *serial)
{
    return guarded<Certificate *>("af_cadb_get_Certificate", nullptr, [&]() -> Certificate * {
        if (!serial)
            fail(EINVALID, "missing serial number");
        Bytes wanted = bytes_of(serial);
        wanted = strip_leading_zeros(wanted.data(), wanted.size());
        for (const auto &e : load(db_path(pse_handle))) {
            Certificate *c = dec_certificate(e.cert);
            if (serial_of(c) == wanted)
                return c;
            free_certificate(c);
        }
        fail(ECADBCERT, "certificate not found in CA database");
    });
}

SET_OF_IssuedCertificate *af_cadb_get_user(PSE pse_handle, Name *name)
{
    return guarded<SET_OF_IssuedCertificate *>("af_cadb_get_user", nullptr, [&] {
        DName *wanted = string_to_dname(name);
        SET_OF_IssuedCertificate *head = nullptr, **tail = &head;
        try {
            for (const auto &e : load(db_path(pse_handle))) {
                CertHolder c(dec_certificate(e.cert));
                if (!dname_equal(c.c->tbs->subject, wanted))
                    continue;
                auto *s = mem_new<SET_OF_IssuedCertificate>();
                *tail = s;
                tail = &s->next;
                s->element = mem_new<IssuedCertificate>();
                s->element->serial = new_ostr(bytes_of(c.c->tbs->serialnumber));
                s->element->date_of_issue = mem_strdup(e.issued);
            }
        } catch (...) {
            free_dname(wanted);
            aux_free_SET_OF_IssuedCertificate(&head);
            throw;
        }
        free_dname(wanted);
        if (!head)
            fail(ECADBUSER, std::string("no certificate registered for ") + name);
        return head;
    });
}

SET_OF_Name *af_cadb_list_user(PSE pse_handle)
{
    return guarded<SET_OF_Name *>("af_cadb_list_user", nullptr, [&] {
        std::vector<std::string> names;
        for (const auto &e : load(db_path(pse_handle))) {
            CertHolder c(dec_certificate(e.cert));
            std::string n = dname_to_string(c.c->tbs->subject);
            if (std::find(names.begin(), names.end(), n) == names.end())
                names.push_back(n);
        }
        SET_OF_Name *head = nullptr, **tail = &head;
        for (const auto &n : names) {
            auto *s = mem_new<SET_OF_Name>();
            s->element = mem_strdup(n);
            *tail = s;
            tail = &s->next;
        }
        if (!head)
            fail(ECADBUSER, "CA database is empty");
        return head;
    });
}

} // extern "C"
