// DER encoding/decoding of SECUDE structures (X.509, PKCS#1, PKCS#8, PKRoot, FCPath).
#include "der.hpp"

#include <ctime>

namespace compat {

namespace {

ParmType parm_type(const ObjId *o)
{
    const AlgInfo *a = alg_by_oid(o);
    return a ? a->parm : NoParmType;
}

Bytes enc_ostr_value(const OctetString &o)
{
    return der::octet_string(bytes_of(&o));
}

Bytes enc_int_part(const OctetString &o)
{
    Bytes b = bytes_of(&o);
    return der::integer_unsigned(b);
}

void fill_part(OctetString &dst, const der::Node &n)
{
    fill_ostr(&dst, der::get_unsigned(n));
}

AlgId *dec_algid_node(const der::Node &node);
DName *dec_dname_node(const der::Node &node);
KeyInfo *dec_keyinfo_node(const der::Node &node);
CertExtensions *dec_extensions_node(const der::Node &node);

} // namespace

std::string utc_now(long offset_seconds)
{
    std::time_t t = std::time(nullptr) + offset_seconds;
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    if (tm.tm_year + 1900 >= 2050)
        std::strftime(buf, sizeof buf, "%Y%m%d%H%M%SZ", &tm);
    else
        std::strftime(buf, sizeof buf, "%y%m%d%H%M%SZ", &tm);
    return buf;
}

// ---- AlgorithmIdentifier ----------------------------------------------------

Bytes enc_algid(const AlgId *a)
{
    if (!a || !a->objid)
        fail(EALGID, "missing algorithm identifier");
    Bytes body = der::oid(oid_of(a->objid));
    ParmType pt = parm_type(a->objid);
    if (a->param) {
        switch (pt) {
        case PARM_INTEGER:
            der::append(body, der::integer(*static_cast<const unsigned int *>(a->param)));
            break;
        case PARM_OctetString:
            der::append(body, der::octet_string(bytes_of(static_cast<const OctetString *>(a->param))));
            break;
        case PARM_KeyBits: {
            auto *k = static_cast<const KeyBits *>(a->param);
            der::append(body, der::seq({enc_int_part(k->part1), enc_int_part(k->part2), enc_int_part(k->part3)}));
            break;
        }
        case PARM_RC2CBC: {
            auto *p = static_cast<const rc2CBC_Parm *>(a->param);
            der::append(body, der::seq({der::integer(p->version), enc_ostr_value(p->IV)}));
            break;
        }
        case PARM_PKCS5: {
            auto *p = static_cast<const PBEParameter *>(a->param);
            der::append(body, der::seq({enc_ostr_value(p->salt), der::integer(p->iterationCount)}));
            break;
        }
        case NoParmType:
            der::append(body, bytes_of(static_cast<const OctetString *>(a->param)));
            break;
        case PARM_NULL:
            der::append(body, der::null());
            break;
        default:
            break;
        }
    } else {
        ParmType effective = a->parmoverride ? *a->parmoverride : pt;
        if (effective == PARM_NULL)
            der::append(body, der::null());
    }
    return der::tlv(der::SEQUENCE, body);
}

namespace {

AlgId *dec_algid_node(const der::Node &node)
{
    if (node.tag != der::SEQUENCE)
        fail(EDECODE, "AlgorithmIdentifier expected");
    der::Reader r(node);
    Oid oid = der::get_oid(r.next());
    AlgId *a = mem_new<AlgId>();
    try {
        a->objid = new_objid(oid);
        ParmType pt = parm_type(a->objid);
        if (r.empty()) {
            if (pt == PARM_NULL) {
                a->parmoverride = mem_new<ParmType>();
                *a->parmoverride = PARM_ABSENT;
            }
            return a;
        }
        der::Node p = r.next();
        r.expect_end();
        if (p.tag == der::NULL_ && pt != NoParmType) {
            if (pt != PARM_NULL) {
                a->parmoverride = mem_new<ParmType>();
                *a->parmoverride = PARM_NULL;
            }
            return a;
        }
        switch (pt) {
        case PARM_INTEGER: {
            unsigned int *v = mem_new<unsigned int>();
            *v = static_cast<unsigned int>(der::get_integer(p));
            a->param = v;
            break;
        }
        case PARM_OctetString:
            if (p.tag != der::OCTET_STRING)
                fail(EDECODE, "OCTET STRING parameter expected");
            a->param = new_ostr(p.value());
            break;
        case PARM_KeyBits: {
            if (p.tag != der::SEQUENCE)
                fail(EDECODE, "domain parameters expected");
            der::Reader pr(p);
            KeyBits *k = mem_new<KeyBits>();
            a->param = k;
            fill_part(k->part1, pr.next(der::INTEGER));
            fill_part(k->part2, pr.next(der::INTEGER));
            fill_part(k->part3, pr.next(der::INTEGER));
            k->choice = 3;
            break;
        }
        case PARM_RC2CBC: {
            auto *rp = mem_new<rc2CBC_Parm>();
            a->param = rp;
            if (p.tag == der::OCTET_STRING) {
                rp->version = 58;
                fill_ostr(&rp->IV, p.value());
            } else {
                der::Reader pr(p);
                der::Node n;
                if (pr.next_optional(der::INTEGER, n))
                    rp->version = int(der::get_integer(n));
                else
                    rp->version = 58;
                fill_ostr(&rp->IV, pr.next(der::OCTET_STRING).value());
            }
            break;
        }
        case PARM_PKCS5: {
            auto *pp = mem_new<PBEParameter>();
            a->param = pp;
            der::Reader pr(p);
            fill_ostr(&pp->salt, pr.next(der::OCTET_STRING).value());
            pp->iterationCount = int(der::get_integer(pr.next(der::INTEGER)));
            break;
        }
        case NoParmType:
            a->param = new_ostr(p.raw());
            break;
        default:
            break;
        }
        return a;
    } catch (...) {
        free_algid(a);
        throw;
    }
}

} // namespace

AlgId *dec_algid(const Bytes &der_bytes)
{
    return dec_algid_node(der::parse_one(der_bytes));
}

// ---- KeyBits / KeyInfo ----------------------------------------------------------

Bytes enc_keybits(const KeyBits *k)
{
    if (!k)
        fail(EINVALID, "missing KeyBits");
    const OctetString *parts[] = {&k->part1, &k->part2, &k->part3, &k->part4, &k->part5};
    int n = k->choice;
    if (n < 1 || n > 5) {
        n = 0;
        while (n < 5 && parts[n]->noctets)
            ++n;
    }
    if (n == 0)
        fail(EINVALID, "empty KeyBits");
    Bytes body;
    for (int i = 0; i < n; ++i)
        der::append(body, enc_int_part(*parts[i]));
    return der::tlv(der::SEQUENCE, body);
}

KeyBits *dec_keybits(const Bytes &der_bytes)
{
    der::Node node = der::parse_one(der_bytes);
    KeyBits *k = mem_new<KeyBits>();
    try {
        OctetString *parts[] = {&k->part1, &k->part2, &k->part3, &k->part4, &k->part5};
        if (node.tag == der::INTEGER) {
            fill_part(k->part1, node);
            k->choice = 1;
            return k;
        }
        if (node.tag != der::SEQUENCE)
            fail(EDECODE, "KeyBits expected");
        der::Reader r(node);
        int n = 0;
        while (!r.empty()) {
            if (n == 5)
                fail(EDECODE, "too many key components");
            fill_part(*parts[n++], r.next(der::INTEGER));
        }
        k->choice = n;
        return k;
    } catch (...) {
        free_keybits_content(k);
        mem_free(k);
        throw;
    }
}

Bytes enc_keyinfo(const KeyInfo *k)
{
    if (!k)
        fail(EINVALID, "missing KeyInfo");
    return der::seq({enc_algid(k->subjectAI),
                     der::bit_string(reinterpret_cast<const uint8_t *>(k->subjectkey.bits), k->subjectkey.nbits)});
}

namespace {

KeyInfo *dec_keyinfo_node(const der::Node &node)
{
    if (node.tag != der::SEQUENCE)
        fail(EDECODE, "SubjectPublicKeyInfo expected");
    der::Reader r(node);
    KeyInfo *k = mem_new<KeyInfo>();
    try {
        k->subjectAI = dec_algid_node(r.next(der::SEQUENCE));
        size_t nbits = 0;
        Bytes bits = der::get_bit_string(r.next(), nbits);
        fill_bstr(&k->subjectkey, bits, nbits);
        r.expect_end();
        return k;
    } catch (...) {
        free_keyinfo(k);
        throw;
    }
}

} // namespace

KeyInfo *dec_keyinfo(const Bytes &der_bytes)
{
    return dec_keyinfo_node(der::parse_one(der_bytes));
}

// ---- Name -------------------------------------------------------------------

Bytes enc_dname(const DName *d)
{
    Bytes body;
    for (; d; d = d->next) {
        std::vector<Bytes> atvs;
        for (const RDName *r = d->element_IF_2; r; r = r->next) {
            const AttrValueAssertion *ava = r->member_IF_0;
            if (!ava || !ava->element_IF_0)
                continue;
            int tag = ava->attr_encoding;
            if (tag <= 0 || tag > 30)
                tag = der::UTF8String;
            Bytes value = bytes_of(ava->element_IF_1);
            atvs.push_back(der::seq({der::oid(oid_of(ava->element_IF_0)), der::tlv(uint8_t(tag), value)}));
        }
        der::append(body, der::set_of(atvs));
    }
    return der::tlv(der::SEQUENCE, body);
}

namespace {

DName *dec_dname_node(const der::Node &node)
{
    if (node.tag != der::SEQUENCE)
        fail(EDECODE, "Name expected");
    DName *head = nullptr, **tail = &head;
    try {
        der::Reader r(node);
        while (!r.empty()) {
            der::Node set = r.next(der::SET);
            DName *dn = mem_new<DName>();
            *tail = dn;
            tail = &dn->next;
            RDName **rtail = &dn->element_IF_2;
            der::Reader sr(set);
            while (!sr.empty()) {
                der::Reader ar(sr.next(der::SEQUENCE));
                RDName *rd = mem_new<RDName>();
                *rtail = rd;
                rtail = &rd->next;
                AttrValueAssertion *ava = mem_new<AttrValueAssertion>();
                rd->member_IF_0 = ava;
                ava->element_IF_0 = new_objid(der::get_oid(ar.next()));
                der::Node v = ar.next();
                ava->element_IF_1 = new_ostr(v.value());
                ava->attr_encoding = static_cast<SEC_attr_encoding>(v.tag & 0x1f);
                ar.expect_end();
            }
        }
        return head;
    } catch (...) {
        free_dname(head);
        throw;
    }
}

} // namespace

DName *dec_dname(const Bytes &der_bytes)
{
    return dec_dname_node(der::parse_one(der_bytes));
}

// ---- Validity, extensions, certificates --------------------------------------

Bytes enc_validity(const Validity *v)
{
    if (!v || !v->notbefore || !v->notafter)
        fail(EINVALID, "missing validity");
    return der::seq({der::time(v->notbefore), der::time(v->notafter)});
}

static Bytes extension(const Oid &id, bool critical, const Bytes &value)
{
    Bytes body = der::oid(id);
    if (critical)
        der::append(body, der::boolean(true));
    der::append(body, der::octet_string(value));
    return der::tlv(der::SEQUENCE, body);
}

Bytes enc_extensions(const CertExtensions *e)
{
    if (!e)
        return {};
    Bytes body;
    if (e->subjectKeyId && e->subjectKeyId->extnValue)
        der::append(body, extension(oids::subjectKeyIdentifier, e->subjectKeyId->critical,
                                    der::octet_string(bytes_of(e->subjectKeyId->extnValue))));
    if (e->authorityKeyId && e->authorityKeyId->extnValue && e->authorityKeyId->extnValue->authorityKeyIdentifier)
        der::append(body, extension(oids::authorityKeyIdentifier, e->authorityKeyId->critical,
                                    der::seq({der::tlv(der::ctx(0, false), bytes_of(e->authorityKeyId->extnValue->authorityKeyIdentifier))})));
    if (e->keyUsage && e->keyUsage->extnValue) {
        const KeyUsage *ku = e->keyUsage->extnValue;
        const Boolean flags[] = {ku->digitalSignature, ku->nonRepudiation, ku->keyEncipherment, ku->dataEncipherment,
                                 ku->keyAgreement, ku->keyCertSign, ku->cRLSign, ku->encipherOnly, ku->decipherOnly};
        uint8_t bits[2] = {0, 0};
        size_t nbits = 0;
        for (size_t i = 0; i < 9; ++i)
            if (flags[i]) {
                bits[i / 8] |= uint8_t(0x80 >> (i % 8));
                nbits = i + 1;
            }
        der::append(body, extension(oids::keyUsage, e->keyUsage->critical, der::bit_string(bits, nbits)));
    }
    if (e->basicConstraints && e->basicConstraints->extnValue) {
        const BasicConstraints *bc = e->basicConstraints->extnValue;
        Bytes v;
        if (bc->cA)
            der::append(v, der::boolean(true));
        if (bc->pathLenConstraint >= 0)
            der::append(v, der::integer(bc->pathLenConstraint));
        der::append(body, extension(oids::basicConstraints, e->basicConstraints->critical, der::tlv(der::SEQUENCE, v)));
    }
    for (const SEQUENCE_OF_Extension *s = e->nonSupported; s; s = s->next) {
        if (!s->element || !s->element->extnId)
            continue;
        der::append(body, extension(oid_of(s->element->extnId), s->element->critical, bytes_of(s->element->extnDERcode)));
    }
    if (body.empty())
        return {};
    return der::tlv(der::SEQUENCE, body);
}

namespace {

CertExtensions *dec_extensions_node(const der::Node &node)
{
    CertExtensions *e = mem_new<CertExtensions>();
    try {
        der::Reader r(node);
        SEQUENCE_OF_Extension **tail = &e->nonSupported;
        int seq = 0;
        while (!r.empty()) {
            der::Reader xr(r.next(der::SEQUENCE));
            auto *n = mem_new<SEQUENCE_OF_Extension>();
            *tail = n;
            tail = &n->next;
            n->element = mem_new<v3Extension>();
            n->element->extnId = new_objid(der::get_oid(xr.next()));
            der::Node c;
            if (xr.next_optional(der::BOOLEAN, c))
                n->element->critical = der::get_boolean(c);
            n->element->extnDERcode = new_ostr(xr.next(der::OCTET_STRING).value());
            n->element->seqnum = seq++;
            xr.expect_end();
        }
        return e;
    } catch (...) {
        free_extensions(e);
        throw;
    }
}

} // namespace

CertExtensions *dec_extensions(const Bytes &der_bytes)
{
    der::Node n = der::parse_one(der_bytes);
    if (n.tag != der::SEQUENCE)
        fail(EDECODE, "Extensions expected");
    return dec_extensions_node(n);
}

Bytes enc_tbs(const ToBeSigned *t)
{
    if (!t)
        fail(EINVALID, "missing ToBeSigned");
    Bytes ext = enc_extensions(t->extensions);
    int version = t->version;
    if (!ext.empty() && version < 2)
        version = 2;
    Bytes body;
    if (version > 0)
        der::append(body, der::tlv(der::ctx(0), der::integer(version)));
    Bytes serial = bytes_of(t->serialnumber);
    if (serial.empty())
        serial.push_back(0);
    der::append(body, der::integer_unsigned(serial));
    der::append(body, enc_algid(t->signatureAI));
    der::append(body, enc_dname(t->issuer));
    der::append(body, enc_validity(t->valid));
    der::append(body, enc_dname(t->subject));
    der::append(body, enc_keyinfo(t->subjectPK));
    if (t->issuerUId) {
        Bytes bs = der::bit_string(reinterpret_cast<const uint8_t *>(t->issuerUId->bits), t->issuerUId->nbits);
        bs[0] = der::ctx(1, false);
        der::append(body, bs);
    }
    if (t->subjectUId) {
        Bytes bs = der::bit_string(reinterpret_cast<const uint8_t *>(t->subjectUId->bits), t->subjectUId->nbits);
        bs[0] = der::ctx(2, false);
        der::append(body, bs);
    }
    if (!ext.empty())
        der::append(body, der::tlv(der::ctx(3), ext));
    return der::tlv(der::SEQUENCE, body);
}

Bytes enc_signature_value(const Signature *s)
{
    if (!s)
        fail(EINVALID, "missing signature");
    return der::bit_string(reinterpret_cast<const uint8_t *>(s->signature.bits), s->signature.nbits);
}

Bytes enc_certificate(const Certificate *c)
{
    if (!c || !c->sig)
        fail(EINVALID, "incomplete certificate");
    Bytes tbs = c->tbs_DERcode && c->tbs_DERcode->noctets ? bytes_of(c->tbs_DERcode) : enc_tbs(c->tbs);
    return der::seq({tbs, enc_algid(c->sig->signAI), enc_signature_value(c->sig)});
}

namespace {

ToBeSigned *dec_tbs_node(const der::Node &node)
{
    if (node.tag != der::SEQUENCE)
        fail(EDECODE, "TBSCertificate expected");
    ToBeSigned *t = mem_new<ToBeSigned>();
    try {
        der::Reader r(node);
        der::Node n;
        if (r.next_optional(der::ctx(0), n)) {
            der::Reader vr(n);
            t->version = int(der::get_integer(vr.next()));
        }
        t->serialnumber = new_ostr(der::get_unsigned(r.next(der::INTEGER)));
        t->signatureAI = dec_algid_node(r.next(der::SEQUENCE));
        t->issuer = dec_dname_node(r.next(der::SEQUENCE));
        der::Reader vr(r.next(der::SEQUENCE));
        t->valid = mem_new<Validity>();
        t->valid->notbefore = mem_strdup(der::get_time(vr.next()));
        t->valid->notafter = mem_strdup(der::get_time(vr.next()));
        t->subject = dec_dname_node(r.next(der::SEQUENCE));
        t->subjectPK = dec_keyinfo_node(r.next(der::SEQUENCE));
        while (!r.empty()) {
            der::Node x = r.next();
            if (x.tag == der::ctx(1, false) || x.tag == der::ctx(2, false)) {
                if (x.len == 0 || x.val[0] > 7)
                    fail(EDECODE, "invalid unique identifier");
                BitString *b = new_bstr(Bytes(x.val + 1, x.val + x.len), (x.len - 1) * 8 - x.val[0]);
                (x.tag == der::ctx(1, false) ? t->issuerUId : t->subjectUId) = b;
            } else if (x.tag == der::ctx(3)) {
                der::Reader er(x);
                t->extensions = dec_extensions_node(er.next(der::SEQUENCE));
            } else {
                fail(EDECODE, "unexpected element in TBSCertificate");
            }
        }
        return t;
    } catch (...) {
        free_tbs(t);
        throw;
    }
}

Certificate *dec_certificate_node(const der::Node &node)
{
    if (node.tag != der::SEQUENCE)
        fail(EDECODE, "Certificate expected");
    Certificate *c = mem_new<Certificate>();
    try {
        der::Reader r(node);
        der::Node tbs = r.next(der::SEQUENCE);
        c->tbs_DERcode = new_ostr(tbs.raw());
        c->tbs = dec_tbs_node(tbs);
        c->sig = mem_new<Signature>();
        c->sig->signAI = dec_algid_node(r.next(der::SEQUENCE));
        size_t nbits = 0;
        Bytes bits = der::get_bit_string(r.next(), nbits);
        fill_bstr(&c->sig->signature, bits, nbits);
        r.expect_end();
        return c;
    } catch (...) {
        free_certificate(c);
        throw;
    }
}

} // namespace

Certificate *dec_certificate(const Bytes &der_bytes)
{
    return dec_certificate_node(der::parse_one(der_bytes));
}

// ---- PKRoot / FCPath (formats of this compatibility layer) --------------------

static Bytes enc_serial_key(const Serial *s)
{
    if (!s || !s->key)
        fail(EINVALID, "incomplete PKRoot key");
    Bytes serial = bytes_of(s->serial);
    if (serial.empty())
        serial.push_back(0);
    Bytes body = der::concat({der::integer_unsigned(serial), der::integer(s->version), enc_keyinfo(s->key)});
    if (s->valid)
        der::append(body, enc_validity(s->valid));
    if (s->sig)
        der::append(body, der::tlv(der::ctx(1), der::seq({enc_algid(s->sig->signAI), enc_signature_value(s->sig)})));
    return der::tlv(der::SEQUENCE, body);
}

static Serial *dec_serial_key(const der::Node &node)
{
    Serial *s = mem_new<Serial>();
    try {
        der::Reader r(node);
        s->serial = new_ostr(der::get_unsigned(r.next(der::INTEGER)));
        s->version = int(der::get_integer(r.next(der::INTEGER)));
        s->key = dec_keyinfo_node(r.next(der::SEQUENCE));
        der::Node n;
        if (r.next_optional(der::SEQUENCE, n)) {
            der::Reader vr(n);
            s->valid = mem_new<Validity>();
            s->valid->notbefore = mem_strdup(der::get_time(vr.next()));
            s->valid->notafter = mem_strdup(der::get_time(vr.next()));
        }
        if (r.next_optional(der::ctx(1), n)) {
            der::Reader sr(der::Reader(n).next(der::SEQUENCE));
            s->sig = mem_new<Signature>();
            s->sig->signAI = dec_algid_node(sr.next(der::SEQUENCE));
            size_t nbits = 0;
            Bytes bits = der::get_bit_string(sr.next(), nbits);
            fill_bstr(&s->sig->signature, bits, nbits);
        }
        r.expect_end();
        return s;
    } catch (...) {
        free_serial(s);
        throw;
    }
}

Bytes enc_pkroot(const PKRoot *p)
{
    if (!p || !p->newkey)
        fail(EINVALID, "incomplete PKRoot");
    Bytes body = der::concat({enc_dname(p->ca), enc_serial_key(p->newkey)});
    if (p->oldkey)
        der::append(body, der::tlv(der::ctx(0), enc_serial_key(p->oldkey)));
    Bytes ext = enc_extensions(p->extensions);
    if (!ext.empty())
        der::append(body, der::tlv(der::ctx(3), ext));
    return der::tlv(der::SEQUENCE, body);
}

PKRoot *dec_pkroot(const Bytes &der_bytes)
{
    der::Node node = der::parse_one(der_bytes);
    if (node.tag != der::SEQUENCE)
        fail(EDECODE, "PKRoot expected");
    PKRoot *p = mem_new<PKRoot>();
    try {
        der::Reader r(node);
        p->ca = dec_dname_node(r.next(der::SEQUENCE));
        p->newkey = dec_serial_key(r.next(der::SEQUENCE));
        der::Node n;
        if (r.next_optional(der::ctx(0), n))
            p->oldkey = dec_serial_key(der::Reader(n).next(der::SEQUENCE));
        if (r.next_optional(der::ctx(3), n))
            p->extensions = dec_extensions_node(der::Reader(n).next(der::SEQUENCE));
        r.expect_end();
        return p;
    } catch (...) {
        free_pkroot(p);
        throw;
    }
}

Bytes enc_fcpath(const FCPath *f)
{
    Bytes body;
    for (; f; f = f->next_forwardpath) {
        std::vector<Bytes> certs;
        for (const SET_OF_Certificate *s = f->liste; s; s = s->next)
            if (s->element)
                certs.push_back(enc_certificate(s->element));
        der::append(body, der::set_of(certs));
    }
    return der::tlv(der::SEQUENCE, body);
}

FCPath *dec_fcpath(const Bytes &der_bytes)
{
    der::Node node = der::parse_one(der_bytes);
    if (node.tag != der::SEQUENCE)
        fail(EDECODE, "FCPath expected");
    FCPath *head = nullptr, **tail = &head;
    try {
        der::Reader r(node);
        while (!r.empty()) {
            FCPath *f = mem_new<FCPath>();
            *tail = f;
            tail = &f->next_forwardpath;
            SET_OF_Certificate **ctail = &f->liste;
            der::Reader sr(r.next(der::SET));
            while (!sr.empty()) {
                auto *e = mem_new<SET_OF_Certificate>();
                *ctail = e;
                ctail = &e->next;
                e->element = dec_certificate_node(sr.next());
            }
        }
        return head;
    } catch (...) {
        free_fcpath(head);
        throw;
    }
}

// ---- PKCS#8 -----------------------------------------------------------------

Bytes enc_privatekeyinfo(const PrivateKeyInfo *p)
{
    if (!p || !p->privateKeyAlgorithm || !p->privateKey)
        fail(EINVALID, "incomplete PrivateKeyInfo");
    return der::seq({der::integer(p->version), enc_algid(p->privateKeyAlgorithm), der::octet_string(bytes_of(p->privateKey))});
}

PrivateKeyInfo *dec_privatekeyinfo(const Bytes &der_bytes)
{
    der::Node node = der::parse_one(der_bytes);
    if (node.tag != der::SEQUENCE)
        fail(EDECODE, "PrivateKeyInfo expected");
    PrivateKeyInfo *p = mem_new<PrivateKeyInfo>();
    try {
        der::Reader r(node);
        p->version = int(der::get_integer(r.next(der::INTEGER)));
        p->privateKeyAlgorithm = dec_algid_node(r.next(der::SEQUENCE));
        p->privateKey = new_ostr(r.next(der::OCTET_STRING).value());
        return p;
    } catch (...) {
        free_privatekeyinfo(p);
        throw;
    }
}

} // namespace compat

using namespace compat;

extern "C" {

OctetString *e_OctetString(OctetString *ostr)
{
    return guarded<OctetString *>("e_OctetString", nullptr, [&]() -> OctetString * {
        if (!ostr)
            fail(EINVALID, "missing OctetString");
        return new_ostr(der::octet_string(bytes_of(ostr)));
    });
}

OctetString *d_OctetString(OctetString *asn1_string)
{
    return guarded<OctetString *>("d_OctetString", nullptr, [&]() -> OctetString * {
        Bytes in = bytes_of(asn1_string);
        der::Node n = der::parse_one(in);
        if (n.tag != der::OCTET_STRING)
            fail(EDECODE, "OCTET STRING expected");
        return new_ostr(n.value());
    });
}

OctetString *e_PrintableString(char *infostruct)
{
    return guarded<OctetString *>("e_PrintableString", nullptr, [&]() -> OctetString * {
        if (!infostruct)
            fail(EINVALID, "missing string");
        return new_ostr(der::string(der::PrintableString, infostruct));
    });
}

char *d_PrintableString(OctetString *asn1string)
{
    return guarded<char *>("d_PrintableString", nullptr, [&] {
        Bytes in = bytes_of(asn1string);
        return mem_strdup(der::get_string(der::parse_one(in)));
    });
}

OctetString *e_AlgId(AlgId *algid)
{
    return guarded<OctetString *>("e_AlgId", nullptr, [&] { return new_ostr(enc_algid(algid)); });
}

BitString *e_KeyBits(KeyBits *kb)
{
    return guarded<BitString *>("e_KeyBits", nullptr, [&] {
        Bytes b = enc_keybits(kb);
        return new_bstr(b, b.size() * 8);
    });
}

KeyBits *d_KeyBits(BitString *s)
{
    return guarded<KeyBits *>("d_KeyBits", nullptr, [&] { return dec_keybits(bytes_of(s)); });
}

OctetString *e_KeyInfo(KeyInfo *ki)
{
    return guarded<OctetString *>("e_KeyInfo", nullptr, [&] { return new_ostr(enc_keyinfo(ki)); });
}

KeyInfo *d_KeyInfo(OctetString *asn1_string)
{
    return guarded<KeyInfo *>("d_KeyInfo", nullptr, [&] { return dec_keyinfo(bytes_of(asn1_string)); });
}

OctetString *e_Certificate(Certificate *certificate)
{
    return guarded<OctetString *>("e_Certificate", nullptr, [&] { return new_ostr(enc_certificate(certificate)); });
}

Certificate *d_Certificate(OctetString *asn1_string)
{
    return guarded<Certificate *>("d_Certificate", nullptr, [&] { return dec_certificate(bytes_of(asn1_string)); });
}

OctetString *e_PKRoot(PKRoot *pkroot)
{
    return guarded<OctetString *>("e_PKRoot", nullptr, [&] { return new_ostr(enc_pkroot(pkroot)); });
}

OctetString *e_FCPath(FCPath *fcpath)
{
    return guarded<OctetString *>("e_FCPath", nullptr, [&] { return new_ostr(enc_fcpath(fcpath)); });
}

RSAPrivateKey *d_RSAPrivateKey(OctetString *asn1_string)
{
    return guarded<RSAPrivateKey *>("d_RSAPrivateKey", nullptr, [&] {
        Bytes in = bytes_of(asn1_string);
        der::Node node = der::parse_one(in);
        if (node.tag != der::SEQUENCE)
            fail(EDECODE, "RSAPrivateKey expected");
        RSAPrivateKey *k = mem_new<RSAPrivateKey>();
        try {
            der::Reader r(node);
            k->version = int(der::get_integer(r.next(der::INTEGER)));
            for (OctetString *o : {&k->modulus, &k->pubex, &k->privex, &k->prime1, &k->prime2, &k->exp1, &k->exp2, &k->coeff})
                fill_ostr(o, der::get_unsigned(r.next(der::INTEGER)));
            return k;
        } catch (...) {
            aux_free_RSAPrivateKey(&k);
            throw;
        }
    });
}

ContentInfo *d_ContentInfo(OctetString *asn1_string)
{
    return guarded<ContentInfo *>("d_ContentInfo", nullptr, [&] {
        Bytes in = bytes_of(asn1_string);
        der::Node node = der::parse_one(in);
        if (node.tag != der::SEQUENCE)
            fail(EDECODE, "ContentInfo expected");
        der::Reader r(node);
        Oid type = der::get_oid(r.next());
        OctetString *content = nullptr;
        der::Node n;
        if (r.next_optional(der::ctx(0), n)) {
            der::Node inner = der::Reader(n).next();
            content = new_ostr(type == oids::data && inner.tag == der::OCTET_STRING ? inner.value() : inner.raw());
        }
        r.expect_end();
        ContentInfo *ci = mem_new<ContentInfo>();
        ci->contentType = new_objid(type);
        ci->content = content;
        return ci;
    });
}

} // extern "C"
