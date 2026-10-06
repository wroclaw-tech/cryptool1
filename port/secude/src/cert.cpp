// X.509 certificates: prototype creation, certification by the CA PSE,
// PKRoot handling and printing.
#include "bn.hpp"
#include "der.hpp"

#include <cctype>
#include <ctime>

namespace compat {

namespace {

constexpr long kDefaultValiditySeconds = 365L * 24 * 3600;

const AlgId *default_sig_alg(const KeyInfo *k)
{
    return keyinfo_is_dsa(k) ? &dsaWithSHA1_aid : &sha1WithRSASignature_aid;
}

void add_extension(CertExtensions *&ext, const Oid &id, bool critical, const Bytes &value)
{
    if (!ext)
        ext = mem_new<CertExtensions>();
    SEQUENCE_OF_Extension **tail = &ext->nonSupported;
    int seq = 0;
    while (*tail) {
        tail = &(*tail)->next;
        ++seq;
    }
    auto *n = mem_new<SEQUENCE_OF_Extension>();
    n->element = mem_new<v3Extension>();
    n->element->extnId = new_objid(id);
    n->element->critical = critical;
    n->element->extnDERcode = new_ostr(value);
    n->element->seqnum = seq;
    *tail = n;
}

Validity *validity_from_now(long seconds)
{
    Validity *v = mem_new<Validity>();
    v->notbefore = mem_strdup(utc_now());
    v->notafter = mem_strdup(utc_now(seconds));
    return v;
}

// Signs t (re-encoded) with priv and returns the finished certificate (takes ownership of t).
Certificate *sign_tbs(ToBeSigned *t, const KeyInfo *priv, const Bytes &e_hint)
{
    Certificate *c = mem_new<Certificate>();
    c->tbs = t;
    try {
        Bytes tbs = enc_tbs(t);
        c->tbs_DERcode = new_ostr(tbs);
        Bytes sig = sign_message(priv, e_hint, t->signatureAI, tbs.data(), tbs.size());
        c->sig = mem_new<Signature>();
        c->sig->signAI = copy_algid(t->signatureAI);
        fill_bstr(&c->sig->signature, sig, sig.size() * 8);
        return c;
    } catch (...) {
        free_certificate(c);
        throw;
    }
}

Bytes increment(Bytes serial)
{
    for (size_t i = serial.size(); i-- > 0;)
        if (++serial[i] != 0)
            return serial;
    serial.insert(serial.begin(), 1);
    return serial;
}

struct KeyInfoHolder {
    KeyInfo *k;
    explicit KeyInfoHolder(KeyInfo *ki) : k(ki) {}
    ~KeyInfoHolder() { free_keyinfo(k); }
    KeyInfoHolder(const KeyInfoHolder &) = delete;
    KeyInfoHolder &operator=(const KeyInfoHolder &) = delete;
};

struct CertHolder {
    Certificate *c;
    explicit CertHolder(Certificate *cert) : c(cert) {}
    ~CertHolder() { free_certificate(c); }
    CertHolder(const CertHolder &) = delete;
    CertHolder &operator=(const CertHolder &) = delete;
};

bool same_serial(const OctetString *a, const OctetString *b)
{
    Bytes x = bytes_of(a), y = bytes_of(b);
    return strip_leading_zeros(x.data(), x.size()) == strip_leading_zeros(y.data(), y.size());
}

} // namespace

std::string format_time(const char *t)
{
    if (!t)
        return "(none)";
    std::string s(t);
    int year, mon, day, hour, min, sec = 0;
    size_t p = 0;
    if (s.size() >= 15 && s[14] == 'Z') {
        year = std::stoi(s.substr(0, 4));
        p = 4;
    } else if (s.size() >= 11) {
        year = std::stoi(s.substr(0, 2));
        year += year < 50 ? 2000 : 1900;
        p = 2;
    } else {
        return s;
    }
    mon = std::stoi(s.substr(p, 2));
    day = std::stoi(s.substr(p + 2, 2));
    hour = std::stoi(s.substr(p + 4, 2));
    min = std::stoi(s.substr(p + 6, 2));
    if (s.size() > p + 9 && std::isdigit(static_cast<unsigned char>(s[p + 8])))
        sec = std::stoi(s.substr(p + 8, 2));
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    std::time_t tt = timegm(&tm);
    std::tm norm{};
    gmtime_r(&tt, &norm);
    char buf[64];
    std::strftime(buf, sizeof buf, "%a %b %d %H:%M:%S %Y UTC", &norm);
    return std::string(buf) + " (" + s + ")";
}

std::string sprint_algid(const AlgId *aid)
{
    if (!aid || !aid->objid)
        return "Algorithm (none)";
    std::string s = "Algorithm " + alg_display_name(aid->objid) + " (OID " + oid_to_string(oid_of(aid->objid)) + ")";
    const AlgInfo *a = alg_by_oid(aid->objid);
    ParmType pt = a ? a->parm : NoParmType;
    if (!aid->param) {
        ParmType effective = aid->parmoverride ? *aid->parmoverride : pt;
        if (effective == PARM_NULL)
            s += ", NULL";
        return s;
    }
    switch (pt) {
    case PARM_INTEGER:
        s += ", Keysize = " + std::to_string(*static_cast<const unsigned int *>(aid->param));
        break;
    case PARM_KeyBits: {
        auto *k = static_cast<const KeyBits *>(aid->param);
        Bytes p = bytes_of(&k->part1);
        s += ", Keysize = " + std::to_string(BN_num_bits(bn_from(p).get()));
        break;
    }
    case PARM_OctetString: {
        Bytes iv = bytes_of(static_cast<const OctetString *>(aid->param));
        s += ", IV = " + hex(iv.data(), iv.size());
        break;
    }
    case PARM_RC2CBC: {
        auto *p = static_cast<const rc2CBC_Parm *>(aid->param);
        Bytes iv = bytes_of(&p->IV);
        s += ", version = " + std::to_string(p->version) + ", IV = " + hex(iv.data(), iv.size());
        break;
    }
    case PARM_PKCS5: {
        auto *p = static_cast<const PBEParameter *>(aid->param);
        Bytes salt = bytes_of(&p->salt);
        s += ", salt = " + hex(salt.data(), salt.size()) + ", iterations = " + std::to_string(p->iterationCount);
        break;
    }
    default: {
        Bytes raw = bytes_of(static_cast<const OctetString *>(aid->param));
        s += ", parameter = " + hex(raw.data(), raw.size());
        break;
    }
    }
    return s;
}

namespace {

std::string sprint_number(const char *label, const Bytes &v, const std::string &indent)
{
    int bits = BN_num_bits(bn_from(v).get());
    Bytes minimal = strip_leading_zeros(v.data(), v.size());
    return indent + label + " (" + std::to_string(bits) + " bits):\n" + hexdump(minimal.data(), minimal.size(), indent + "   ", false);
}

std::string sprint_keyinfo(const KeyInfo *k, uint32_t flags, const std::string &indent)
{
    std::string s;
    if (!flags || (flags & ALGID))
        s += indent + sprint_algid(k->subjectAI) + "\n";
    if (flags & BITSTRING) {
        Bytes b = bytes_of(&k->subjectkey);
        s += indent + "Key BIT STRING (" + std::to_string(k->subjectkey.nbits) + " bits):\n" +
             hexdump(b.data(), b.size(), indent + "   ", false);
    }
    if (!flags || (flags & KEYBITS)) {
        try {
            if (keyinfo_is_rsa(k)) {
                KeyBits *kb = dec_keybits(bytes_of(&k->subjectkey));
                Bytes p1 = bytes_of(&kb->part1), p2 = bytes_of(&kb->part2);
                free_keybits_content(kb);
                mem_free(kb);
                s += sprint_number("Modulus", p1, indent);
                s += sprint_number("Public exponent", p2, indent);
            } else if (keyinfo_is_dsa(k)) {
                if (k->subjectAI->param) {
                    auto *params = static_cast<const KeyBits *>(k->subjectAI->param);
                    s += sprint_number("Prime p", bytes_of(&params->part1), indent);
                    s += sprint_number("Subprime q", bytes_of(&params->part2), indent);
                    s += sprint_number("Base g", bytes_of(&params->part3), indent);
                }
                Bytes y = bytes_of(&k->subjectkey);
                s += sprint_number("Public key y", der::get_unsigned(der::parse_one(y)), indent);
            }
        } catch (const Error &) {
            s += indent + "(undecodable key)\n";
        }
    }
    return s;
}

std::string sprint_extension(const v3Extension *x, const std::string &indent)
{
    Oid id = oid_of(x->extnId);
    Bytes v = bytes_of(x->extnDERcode);
    std::string name, value;
    try {
        if (id == oids::cryptoolPseName) {
            name = "CrypToolPSEName";
            value = der::get_string(der::parse_one(v));
        } else if (id == oids::basicConstraints) {
            name = "BasicConstraints";
            der::Reader r(der::parse_one(v));
            der::Node n;
            bool ca = r.next_optional(der::BOOLEAN, n) && der::get_boolean(n);
            value = ca ? "CA:TRUE" : "CA:FALSE";
            if (r.next_optional(der::INTEGER, n))
                value += ", pathlen:" + std::to_string(der::get_integer(n));
        } else if (id == oids::keyUsage) {
            name = "KeyUsage";
            size_t nbits = 0;
            Bytes bits = der::get_bit_string(der::parse_one(v), nbits);
            static const char *names[] = {"digitalSignature", "nonRepudiation", "keyEncipherment",
                                          "dataEncipherment", "keyAgreement",   "keyCertSign",
                                          "cRLSign",          "encipherOnly",   "decipherOnly"};
            for (size_t i = 0; i < nbits && i < 9; ++i)
                if (bits[i / 8] & (0x80 >> (i % 8)))
                    value += (value.empty() ? "" : ", ") + std::string(names[i]);
        } else if (id == oids::subjectKeyIdentifier) {
            name = "SubjectKeyIdentifier";
            Bytes kid = der::parse_one(v).value();
            value = hex(kid.data(), kid.size());
        } else {
            name = oid_to_string(id);
            value = hex(v.data(), v.size());
        }
    } catch (const Error &) {
        name = oid_to_string(id);
        value = hex(v.data(), v.size());
    }
    return indent + name + (x->critical ? " (critical)" : "") + ": " + value + "\n";
}

std::string sprint_certificate(const Certificate *c)
{
    if (!c || !c->tbs)
        fail(EINVALID, "missing certificate");
    uint32_t f = print_cert_flags();
    uint32_t kf = print_keyinfo_flags() & 0xff;
    const ToBeSigned *t = c->tbs;
    std::string s = "Certificate:\n";
    const std::string in = "   ";
    if (!f || (f & VER))
        s += in + "Version:              " + std::to_string(t->version + 1) + "\n";
    if (!f || (f & TBS)) {
        Bytes serial = bytes_of(t->serialnumber);
        s += in + "SerialNumber:         " + hex(serial.data(), serial.size()) + "\n";
    }
    if (!f || (f & ALG))
        s += in + "Signature algorithm:  " + sprint_algid(t->signatureAI) + "\n";
    if (!f || (f & ISSUER))
        s += in + "Issuer:               " + dname_to_string(t->issuer) + "\n";
    if ((!f || (f & VAL)) && t->valid) {
        s += in + "Validity:\n";
        s += in + "   NotBefore:         " + format_time(t->valid->notbefore) + "\n";
        s += in + "   NotAfter:          " + format_time(t->valid->notafter) + "\n";
    }
    if (!f || (f & SUBJECT))
        s += in + "Subject:              " + dname_to_string(t->subject) + "\n";
    if ((!f || (f & KEYINFO)) && t->subjectPK)
        s += in + "SubjectPublicKeyInfo:\n" + sprint_keyinfo(t->subjectPK, kf, in + "   ");
    if ((!f || (f & EXTENSIONS)) && t->extensions && t->extensions->nonSupported) {
        s += in + "Extensions:\n";
        for (const SEQUENCE_OF_Extension *x = t->extensions->nonSupported; x; x = x->next)
            if (x->element)
                s += sprint_extension(x->element, in + "   ");
    }
    if ((!f || (f & SIGNAT)) && c->sig) {
        Bytes sig = bytes_of(&c->sig->signature);
        s += in + "Signature (" + alg_display_name(c->sig->signAI ? c->sig->signAI->objid : nullptr) + "):\n" +
             hexdump(sig.data(), sig.size(), in + "   ", false);
    }
    if (f & (HSH | DER)) {
        Bytes der_bytes = enc_certificate(c);
        if (f & HSH) {
            Bytes md5 = digest(Hash::MD5, der_bytes.data(), der_bytes.size());
            Bytes sha1 = digest(Hash::SHA1, der_bytes.data(), der_bytes.size());
            s += in + "Fingerprint (MD5):    " + hex(md5.data(), md5.size()) + "\n";
            s += in + "Fingerprint (SHA-1):  " + hex(sha1.data(), sha1.size()) + "\n";
        }
        if (f & DER)
            s += in + "DER code:\n" + hexdump(der_bytes.data(), der_bytes.size(), in + "   ", true);
    }
    return s;
}

const Certificate *find_in_fcpath(const FCPath *f, const DName *issuer, const OctetString *serial)
{
    for (; f; f = f->next_forwardpath)
        for (const SET_OF_Certificate *s = f->liste; s; s = s->next)
            if (s->element && s->element->tbs && dname_equal(s->element->tbs->issuer, issuer) &&
                same_serial(s->element->tbs->serialnumber, serial))
                return s->element;
    return nullptr;
}

} // namespace

Certificate *make_ca_certificate(PSE pse, const KeyInfo *pub, const AlgId *sig_alg, const DName *subject,
                                 long validity_seconds)
{
    auto store = store_of(pse);
    KeyInfoHolder priv(read_private_key(pse, SIGNATURE));
    Bytes e_hint;
    if (keyinfo_is_rsa(pub))
        e_hint = rsa_public_from_keyinfo(pub).e;
    ToBeSigned *t = mem_new<ToBeSigned>();
    try {
        t->version = 2;
        t->serialnumber = new_ostr(Bytes{0});
        t->signatureAI = copy_algid(sig_alg);
        t->issuer = copy_dname(subject);
        t->subject = copy_dname(subject);
        t->valid = validity_from_now(validity_seconds);
        t->subjectPK = copy_keyinfo(pub);
        add_extension(t->extensions, oids::basicConstraints, true, der::seq({der::boolean(true)}));
        const uint8_t usage[1] = {0x06};
        add_extension(t->extensions, oids::keyUsage, true, der::bit_string(usage, 7));
    } catch (...) {
        free_tbs(t);
        throw;
    }
    return sign_tbs(t, priv.k, e_hint);
}

} // namespace compat

using namespace compat;

extern "C" {

Certificate *af_create_Certificate(PSE pse_handle, KeyInfo *keyinfo, AlgId *sig_alg, char *obj_name, DName *subject,
                                   OctetString *serial, GeneralNames *subject_names, KeyIdentifier *subjKeyId,
                                   Boolean CA, KeyUsage *key_usage)
{
    return guarded<Certificate *>("af_create_Certificate", nullptr, [&] {
        auto store = store_of(pse_handle);
        if (!keyinfo)
            fail(EINVALID, "missing public key");
        if (!subject)
            fail(ENONAME, "missing subject name");
        if (subject_names)
            fail(ENOTSUPPORTED, "subject alternative names are not supported");
        std::string object = obj_name && *obj_name ? obj_name : private_key_object(pse_handle, SIGNATURE);
        const PseObject *o = store_find(store, object);
        if (!o)
            fail(EPSEOBJECTNOTEXISTING, "PSE object " + object + " does not exist");
        KeyInfoHolder priv(dec_keyinfo(o->value));
        Bytes e_hint;
        if (keyinfo_is_rsa(keyinfo))
            e_hint = rsa_public_from_keyinfo(keyinfo).e;

        ToBeSigned *t = mem_new<ToBeSigned>();
        try {
            Bytes sn = serial && serial->noctets ? bytes_of(serial) : Bytes{1};
            t->serialnumber = new_ostr(sn);
            t->signatureAI = copy_algid(sig_alg ? sig_alg : default_sig_alg(priv.k));
            t->issuer = copy_dname(subject);
            t->subject = copy_dname(subject);
            t->valid = validity_from_now(kDefaultValiditySeconds);
            t->subjectPK = copy_keyinfo(keyinfo);
            if (subjKeyId)
                add_extension(t->extensions, oids::subjectKeyIdentifier, false, der::octet_string(bytes_of(subjKeyId)));
            if (key_usage) {
                CertExtensions tmp{};
                Ext_KeyUsage ku{};
                ku.critical = TRUE;
                ku.extnValue = key_usage;
                tmp.keyUsage = &ku;
                CertExtensions *norm = dec_extensions(enc_extensions(&tmp));
                add_extension(t->extensions, oids::keyUsage, true, bytes_of(norm->nonSupported->element->extnDERcode));
                free_extensions(norm);
            }
            t->version = t->extensions ? 2 : 0;
        } catch (...) {
            free_tbs(t);
            throw;
        }
        return sign_tbs(t, priv.k, e_hint);
    });
}

Certificate *af_certify(PSE pse_handle, Certificate *tbc_cert, Validity *validity, AlgId *sig_alg, DName *name)
{
    return guarded<Certificate *>("af_certify", nullptr, [&] {
        auto store = store_of(pse_handle);
        if (!tbc_cert || !tbc_cert->tbs || !tbc_cert->tbs->subjectPK)
            fail(EINVALID, "incomplete prototype certificate");
        KeyInfoHolder priv(read_private_key(pse_handle, SIGNATURE));
        const PseObject *own = store_find(store, certificate_object(pse_handle, SIGNATURE));
        if (!own)
            fail(EROOTCERT, "the CA PSE holds no certificate");
        CertHolder ca(dec_certificate(own->value));

        const PseObject *sn = store_find(store, SerialNumber_name);
        Bytes serial = sn && !sn->value.empty() ? sn->value : Bytes{1};

        ToBeSigned *t = mem_new<ToBeSigned>();
        try {
            t->serialnumber = new_ostr(serial);
            t->signatureAI = copy_algid(sig_alg ? sig_alg : default_sig_alg(priv.k));
            t->issuer = copy_dname(ca.c->tbs->subject);
            t->subject = copy_dname(name ? name : tbc_cert->tbs->subject);
            t->valid = validity ? copy_validity(validity) : validity_from_now(kDefaultValiditySeconds);
            t->subjectPK = copy_keyinfo(tbc_cert->tbs->subjectPK);
            Bytes ext = enc_extensions(tbc_cert->tbs->extensions);
            if (!ext.empty())
                t->extensions = dec_extensions(ext);
            t->version = t->extensions ? 2 : 0;
        } catch (...) {
            free_tbs(t);
            throw;
        }
        Certificate *c = sign_tbs(t, priv.k, rsa_exponent_hint(pse_handle, priv.k));
        try {
            store_put(store, SerialNumber_name, oid_of(&SerialNumber_oid), increment(serial));
        } catch (...) {
            free_certificate(c);
            throw;
        }
        return c;
    });
}

PKRoot *aux_create_PKRoot(Certificate *cert1, Certificate *cert2)
{
    return guarded<PKRoot *>("aux_create_PKRoot", nullptr, [&] {
        if (!cert1 || !cert1->tbs)
            fail(EINVALID, "missing root certificate");
        auto make = [](const Certificate *c) {
            Serial *s = mem_new<Serial>();
            s->serial = new_ostr(bytes_of(c->tbs->serialnumber));
            s->version = c->tbs->version;
            s->key = copy_keyinfo(c->tbs->subjectPK);
            s->valid = copy_validity(c->tbs->valid);
            s->sig = copy_signature(c->sig);
            return s;
        };
        PKRoot *p = mem_new<PKRoot>();
        p->ca = copy_dname(cert1->tbs->subject);
        p->newkey = make(cert1);
        if (cert2 && cert2->tbs)
            p->oldkey = make(cert2);
        p->extensions = copy_extensions(cert1->tbs->extensions);
        return p;
    });
}

Certificate *af_PKRoot2Protocert(PKRoot *pkroot)
{
    return guarded<Certificate *>("af_PKRoot2Protocert", nullptr, [&] {
        if (!pkroot || !pkroot->newkey || !pkroot->newkey->key)
            fail(EINVALID, "incomplete PKRoot");
        const Serial *k = pkroot->newkey;
        Certificate *c = mem_new<Certificate>();
        try {
            ToBeSigned *t = mem_new<ToBeSigned>();
            c->tbs = t;
            t->version = k->version;
            t->serialnumber = new_ostr(bytes_of(k->serial));
            t->signatureAI = copy_algid(k->sig ? k->sig->signAI : &sha1WithRSASignature_aid);
            t->issuer = copy_dname(pkroot->ca);
            t->subject = copy_dname(pkroot->ca);
            t->valid = k->valid ? copy_validity(k->valid) : validity_from_now(kDefaultValiditySeconds);
            t->subjectPK = copy_keyinfo(k->key);
            t->extensions = copy_extensions(pkroot->extensions);
            c->tbs_DERcode = new_ostr(enc_tbs(t));
            c->sig = copy_signature(k->sig);
            if (!c->sig) {
                c->sig = mem_new<Signature>();
                c->sig->signAI = copy_algid(t->signatureAI);
            }
            return c;
        } catch (...) {
            free_certificate(c);
            throw;
        }
    });
}

Certificate *af_pse_get_Certificate(PSE pse_handle, KeyType type, DName *issuer, OctetString *serial)
{
    return guarded<Certificate *>("af_pse_get_Certificate", nullptr, [&]() -> Certificate * {
        auto store = store_of(pse_handle);
        if (!issuer && !serial) {
            const PseObject *o = store_find(store, certificate_object(pse_handle, type));
            if (!o)
                fail(EPSEOBJECTNOTEXISTING, "PSE holds no certificate");
            return dec_certificate(o->value);
        }
        for (const char *name : {Cert_name, SignCert_name, EncCert_name}) {
            const PseObject *o = store_find(store, name);
            if (!o)
                continue;
            Certificate *c = dec_certificate(o->value);
            if ((!issuer || dname_equal(c->tbs->issuer, issuer)) && (!serial || same_serial(c->tbs->serialnumber, serial)))
                return c;
            free_certificate(c);
        }
        if (const PseObject *o = store_find(store, FCPath_name)) {
            FCPath *f = dec_fcpath(o->value);
            const Certificate *found = find_in_fcpath(f, issuer, serial);
            Certificate *c = found ? copy_certificate(found) : nullptr;
            free_fcpath(f);
            if (c)
                return c;
        }
        fail(EPSEOBJECTNOTEXISTING, "certificate not found in PSE");
    });
}

Certificates *af_pse_get_Certificates(PSE pse_handle, KeyType type, DName *name)
{
    return guarded<Certificates *>("af_pse_get_Certificates", nullptr, [&] {
        auto store = store_of(pse_handle);
        const PseObject *o = store_find(store, certificate_object(pse_handle, type));
        if (!o)
            fail(EPSEOBJECTNOTEXISTING, "PSE holds no certificate");
        Certificates *c = mem_new<Certificates>();
        try {
            c->usercertificate = dec_certificate(o->value);
            if (const PseObject *f = store_find(store, FCPath_name))
                c->forwardpath = dec_fcpath(f->value);
            return c;
        } catch (...) {
            aux_free_Certificates(&c);
            throw;
        }
    });
}

char *aux_sprint_Certificate(PSE pse_handle, char *string, Certificate *cert)
{
    return guarded<char *>("aux_sprint_Certificate", nullptr, [&] { return append_string(string, sprint_certificate(cert)); });
}

char *aux_sprint_AlgId(char *string, AlgId *aid)
{
    return guarded<char *>("aux_sprint_AlgId", nullptr, [&] { return append_string(string, sprint_algid(aid)); });
}

} // extern "C"
