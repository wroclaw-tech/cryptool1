// Deep copy and release of SECUDE structures.
#include "internal.hpp"

namespace compat {

namespace {

ParmType parm_type_of(const ObjId *o)
{
    const AlgInfo *a = alg_by_oid(o);
    return a ? a->parm : NoParmType;
}

void copy_ostr_into(OctetString *dst, const OctetString *src)
{
    fill_ostr(dst, bytes_of(src));
}

char *copy_str(const char *s) { return mem_strdup(s); }

} // namespace

AlgId *copy_algid(const AlgId *a)
{
    if (!a)
        return nullptr;
    AlgId *r = mem_new<AlgId>();
    r->objid = copy_objid(a->objid);
    if (a->parmoverride) {
        r->parmoverride = mem_new<ParmType>();
        *r->parmoverride = *a->parmoverride;
    }
    if (!a->param)
        return r;
    switch (parm_type_of(a->objid)) {
    case PARM_INTEGER: {
        unsigned int *v = mem_new<unsigned int>();
        *v = *static_cast<const unsigned int *>(a->param);
        r->param = v;
        break;
    }
    case PARM_OctetString:
    case NoParmType:
        r->param = new_ostr(bytes_of(static_cast<const OctetString *>(a->param)));
        break;
    case PARM_KeyBits:
        r->param = copy_keybits(static_cast<const KeyBits *>(a->param));
        break;
    case PARM_RC2CBC: {
        auto *src = static_cast<const rc2CBC_Parm *>(a->param);
        auto *p = mem_new<rc2CBC_Parm>();
        p->version = src->version;
        copy_ostr_into(&p->IV, &src->IV);
        r->param = p;
        break;
    }
    case PARM_PKCS5: {
        auto *src = static_cast<const PBEParameter *>(a->param);
        auto *p = mem_new<PBEParameter>();
        p->iterationCount = src->iterationCount;
        copy_ostr_into(&p->salt, &src->salt);
        r->param = p;
        break;
    }
    default:
        break;
    }
    return r;
}

void free_algid_content(AlgId *a)
{
    if (!a)
        return;
    if (a->param && mem_owned(a->param)) {
        switch (parm_type_of(a->objid)) {
        case PARM_OctetString:
        case NoParmType: {
            OctetString *o = static_cast<OctetString *>(a->param);
            aux_free_OctetString(&o);
            break;
        }
        case PARM_KeyBits: {
            KeyBits *k = static_cast<KeyBits *>(a->param);
            free_keybits_content(k);
            mem_free(k);
            break;
        }
        case PARM_RC2CBC:
            aux_free2_OctetString(&static_cast<rc2CBC_Parm *>(a->param)->IV);
            mem_free(a->param);
            break;
        case PARM_PKCS5:
            aux_free2_OctetString(&static_cast<PBEParameter *>(a->param)->salt);
            mem_free(a->param);
            break;
        default:
            mem_free(a->param);
            break;
        }
        a->param = nullptr;
    }
    ObjId *o = a->objid;
    aux_free_ObjId(&o);
    a->objid = nullptr;
    mem_free(a->parmoverride);
    a->parmoverride = nullptr;
}

void free_algid(AlgId *a)
{
    if (!mem_owned(a))
        return;
    if (!a)
        return;
    free_algid_content(a);
    mem_free(a);
}

KeyBits *copy_keybits(const KeyBits *k)
{
    if (!k)
        return nullptr;
    KeyBits *r = mem_new<KeyBits>();
    copy_ostr_into(&r->part1, &k->part1);
    copy_ostr_into(&r->part2, &k->part2);
    copy_ostr_into(&r->part3, &k->part3);
    copy_ostr_into(&r->part4, &k->part4);
    copy_ostr_into(&r->part5, &k->part5);
    r->choice = k->choice;
    return r;
}

void free_keybits_content(KeyBits *k)
{
    if (!k)
        return;
    aux_free2_OctetString(&k->part1);
    aux_free2_OctetString(&k->part2);
    aux_free2_OctetString(&k->part3);
    aux_free2_OctetString(&k->part4);
    aux_free2_OctetString(&k->part5);
}

KeyInfo *copy_keyinfo(const KeyInfo *k)
{
    if (!k)
        return nullptr;
    KeyInfo *r = mem_new<KeyInfo>();
    r->subjectAI = copy_algid(k->subjectAI);
    fill_bstr(&r->subjectkey, bytes_of(&k->subjectkey), k->subjectkey.nbits);
    return r;
}

void free_keyinfo_content(KeyInfo *k)
{
    if (!k)
        return;
    free_algid(k->subjectAI);
    k->subjectAI = nullptr;
    aux_free2_BitString(&k->subjectkey);
}

void free_keyinfo(KeyInfo *k)
{
    if (!mem_owned(k))
        return;
    if (!k)
        return;
    free_keyinfo_content(k);
    mem_free(k);
}

DName *copy_dname(const DName *d)
{
    DName *head = nullptr, **tail = &head;
    for (; d; d = d->next) {
        DName *n = mem_new<DName>();
        RDName **rtail = &n->element_IF_2;
        for (const RDName *r = d->element_IF_2; r; r = r->next) {
            RDName *nr = mem_new<RDName>();
            if (r->member_IF_0) {
                AttrValueAssertion *ava = mem_new<AttrValueAssertion>();
                ava->element_IF_0 = copy_objid(r->member_IF_0->element_IF_0);
                if (r->member_IF_0->element_IF_1)
                    ava->element_IF_1 = new_ostr(bytes_of(r->member_IF_0->element_IF_1));
                ava->attr_encoding = r->member_IF_0->attr_encoding;
                nr->member_IF_0 = ava;
            }
            *rtail = nr;
            rtail = &nr->next;
        }
        *tail = n;
        tail = &n->next;
    }
    return head;
}

void free_dname(DName *d)
{
    while (d && mem_owned(d)) {
        DName *next = d->next;
        RDName *r = d->element_IF_2;
        while (r) {
            RDName *rn = r->next;
            if (r->member_IF_0) {
                aux_free_ObjId(&r->member_IF_0->element_IF_0);
                aux_free_OctetString(&r->member_IF_0->element_IF_1);
                mem_free(r->member_IF_0);
            }
            mem_free(r);
            r = rn;
        }
        mem_free(d);
        d = next;
    }
}

Validity *copy_validity(const Validity *v)
{
    if (!v)
        return nullptr;
    Validity *r = mem_new<Validity>();
    r->notbefore = copy_str(v->notbefore);
    r->notafter = copy_str(v->notafter);
    return r;
}

void free_validity(Validity *v)
{
    if (!mem_owned(v))
        return;
    if (!v)
        return;
    mem_free(v->notbefore);
    mem_free(v->notafter);
    mem_free(v);
}

Signature *copy_signature(const Signature *s)
{
    if (!s)
        return nullptr;
    Signature *r = mem_new<Signature>();
    r->signAI = copy_algid(s->signAI);
    fill_bstr(&r->signature, bytes_of(&s->signature), s->signature.nbits);
    return r;
}

void free_signature(Signature *s)
{
    if (!mem_owned(s))
        return;
    if (!s)
        return;
    free_algid(s->signAI);
    aux_free2_BitString(&s->signature);
    mem_free(s);
}

CertExtensions *copy_extensions(const CertExtensions *e)
{
    if (!e)
        return nullptr;
    CertExtensions *r = mem_new<CertExtensions>();
    SEQUENCE_OF_Extension **tail = &r->nonSupported;
    for (const SEQUENCE_OF_Extension *s = e->nonSupported; s; s = s->next) {
        if (!s->element)
            continue;
        auto *n = mem_new<SEQUENCE_OF_Extension>();
        n->element = mem_new<v3Extension>();
        n->element->extnId = copy_objid(s->element->extnId);
        n->element->critical = s->element->critical;
        if (s->element->extnDERcode)
            n->element->extnDERcode = new_ostr(bytes_of(s->element->extnDERcode));
        n->element->seqnum = s->element->seqnum;
        *tail = n;
        tail = &n->next;
    }
    return r;
}

void free_extensions(CertExtensions *e)
{
    if (!mem_owned(e))
        return;
    if (!e)
        return;
    SEQUENCE_OF_Extension *s = e->nonSupported;
    while (s) {
        SEQUENCE_OF_Extension *next = s->next;
        if (s->element) {
            aux_free_ObjId(&s->element->extnId);
            aux_free_OctetString(&s->element->extnDERcode);
            mem_free(s->element);
        }
        mem_free(s);
        s = next;
    }
    mem_free(e);
}

ToBeSigned *copy_tbs(const ToBeSigned *t)
{
    if (!t)
        return nullptr;
    ToBeSigned *r = mem_new<ToBeSigned>();
    r->version = t->version;
    if (t->serialnumber)
        r->serialnumber = new_ostr(bytes_of(t->serialnumber));
    r->signatureAI = copy_algid(t->signatureAI);
    r->issuer = copy_dname(t->issuer);
    r->valid = copy_validity(t->valid);
    r->subject = copy_dname(t->subject);
    r->subjectPK = copy_keyinfo(t->subjectPK);
    if (t->issuerUId)
        r->issuerUId = new_bstr(bytes_of(t->issuerUId), t->issuerUId->nbits);
    if (t->subjectUId)
        r->subjectUId = new_bstr(bytes_of(t->subjectUId), t->subjectUId->nbits);
    r->extensions = copy_extensions(t->extensions);
    return r;
}

void free_tbs(ToBeSigned *t)
{
    if (!mem_owned(t))
        return;
    if (!t)
        return;
    aux_free_OctetString(&t->serialnumber);
    free_algid(t->signatureAI);
    free_dname(t->issuer);
    free_validity(t->valid);
    free_dname(t->subject);
    free_keyinfo(t->subjectPK);
    aux_free_BitString(&t->issuerUId);
    aux_free_BitString(&t->subjectUId);
    free_extensions(t->extensions);
    mem_free(t);
}

Certificate *copy_certificate(const Certificate *c)
{
    if (!c)
        return nullptr;
    Certificate *r = mem_new<Certificate>();
    if (c->tbs_DERcode)
        r->tbs_DERcode = new_ostr(bytes_of(c->tbs_DERcode));
    r->tbs = copy_tbs(c->tbs);
    r->sig = copy_signature(c->sig);
    return r;
}

void free_certificate(Certificate *c)
{
    if (!mem_owned(c))
        return;
    if (!c)
        return;
    aux_free_OctetString(&c->tbs_DERcode);
    free_tbs(c->tbs);
    free_signature(c->sig);
    mem_free(c);
}

FCPath *copy_fcpath(const FCPath *f)
{
    FCPath *head = nullptr, **tail = &head;
    for (; f; f = f->next_forwardpath) {
        FCPath *n = mem_new<FCPath>();
        SET_OF_Certificate **ctail = &n->liste;
        for (const SET_OF_Certificate *s = f->liste; s; s = s->next) {
            if (!s->element)
                continue;
            auto *e = mem_new<SET_OF_Certificate>();
            e->element = copy_certificate(s->element);
            *ctail = e;
            ctail = &e->next;
        }
        *tail = n;
        tail = &n->next_forwardpath;
    }
    return head;
}

void free_fcpath(FCPath *f)
{
    while (f && mem_owned(f)) {
        FCPath *next = f->next_forwardpath;
        SET_OF_Certificate *s = f->liste;
        while (s) {
            SET_OF_Certificate *sn = s->next;
            free_certificate(s->element);
            mem_free(s);
            s = sn;
        }
        mem_free(f);
        f = next;
    }
}

static Serial *copy_serial(const Serial *s)
{
    if (!s)
        return nullptr;
    Serial *r = mem_new<Serial>();
    if (s->serial)
        r->serial = new_ostr(bytes_of(s->serial));
    r->version = s->version;
    r->key = copy_keyinfo(s->key);
    r->valid = copy_validity(s->valid);
    r->sig = copy_signature(s->sig);
    return r;
}

void free_serial(Serial *s)
{
    if (!mem_owned(s))
        return;
    if (!s)
        return;
    aux_free_OctetString(&s->serial);
    free_keyinfo(s->key);
    free_validity(s->valid);
    free_signature(s->sig);
    mem_free(s);
}

PKRoot *copy_pkroot(const PKRoot *p)
{
    if (!p)
        return nullptr;
    PKRoot *r = mem_new<PKRoot>();
    r->ca = copy_dname(p->ca);
    r->newkey = copy_serial(p->newkey);
    r->oldkey = copy_serial(p->oldkey);
    r->extensions = copy_extensions(p->extensions);
    return r;
}

void free_pkroot(PKRoot *p)
{
    if (!mem_owned(p))
        return;
    if (!p)
        return;
    free_dname(p->ca);
    free_serial(p->newkey);
    free_serial(p->oldkey);
    free_extensions(p->extensions);
    mem_free(p);
}

void free_privatekeyinfo(PrivateKeyInfo *p)
{
    if (!mem_owned(p))
        return;
    if (!p)
        return;
    free_algid(p->privateKeyAlgorithm);
    aux_free_OctetString(&p->privateKey);
    mem_free(p);
}

} // namespace compat

using namespace compat;

extern "C" {

AlgId *aux_cpy_AlgId(AlgId *aid)
{
    return guarded<AlgId *>("aux_cpy_AlgId", nullptr, [&] { return copy_algid(aid); });
}

int aux_cpy2_AlgId(AlgId *dup_aid, AlgId *aid)
{
    return guarded<int>("aux_cpy2_AlgId", -1, [&] {
        if (!dup_aid || !aid)
            fail(EINVALID, "missing algorithm identifier");
        AlgId *c = copy_algid(aid);
        *dup_aid = *c;
        mem_free(c);
        return 0;
    });
}

void aux_free2_AlgId(AlgId *algid) { free_algid_content(algid); }

void aux_free_AlgId(AlgId **aid)
{
    if (!aid || !*aid)
        return;
    if (!mem_owned(*aid)) {
        *aid = nullptr;
        return;
    }
    free_algid(*aid);
    *aid = nullptr;
}

void aux_free2_KeyBits(KeyBits *p) { free_keybits_content(p); }

void aux_free_KeyBits(KeyBits **kb)
{
    if (!kb || !*kb)
        return;
    if (!mem_owned(*kb)) {
        *kb = nullptr;
        return;
    }
    free_keybits_content(*kb);
    mem_free(*kb);
    *kb = nullptr;
}

void aux_free2_KeyInfo(KeyInfo *p) { free_keyinfo_content(p); }

void aux_free_KeyInfo(KeyInfo **ki)
{
    if (!ki || !*ki)
        return;
    if (!mem_owned(*ki)) {
        *ki = nullptr;
        return;
    }
    free_keyinfo(*ki);
    *ki = nullptr;
}

void aux_free_DName(DName **dn)
{
    if (!dn || !*dn)
        return;
    if (!mem_owned(*dn)) {
        *dn = nullptr;
        return;
    }
    free_dname(*dn);
    *dn = nullptr;
}

void aux_free_Certificate(Certificate **cert)
{
    if (!cert || !*cert)
        return;
    if (!mem_owned(*cert)) {
        *cert = nullptr;
        return;
    }
    free_certificate(*cert);
    *cert = nullptr;
}

void aux_free_Certificates(Certificates **to_be_freed)
{
    if (!to_be_freed || !*to_be_freed)
        return;
    if (!mem_owned(*to_be_freed)) {
        *to_be_freed = nullptr;
        return;
    }
    free_certificate((*to_be_freed)->usercertificate);
    free_fcpath((*to_be_freed)->forwardpath);
    mem_free(*to_be_freed);
    *to_be_freed = nullptr;
}

void aux_free_FCPath(FCPath **path)
{
    if (!path || !*path)
        return;
    if (!mem_owned(*path)) {
        *path = nullptr;
        return;
    }
    free_fcpath(*path);
    *path = nullptr;
}

void aux_free_PKRoot(PKRoot **pkroot)
{
    if (!pkroot || !*pkroot)
        return;
    if (!mem_owned(*pkroot)) {
        *pkroot = nullptr;
        return;
    }
    free_pkroot(*pkroot);
    *pkroot = nullptr;
}

void aux_free_RSAPrivateKey(RSAPrivateKey **privkey)
{
    if (!privkey || !*privkey)
        return;
    if (!mem_owned(*privkey)) {
        *privkey = nullptr;
        return;
    }
    RSAPrivateKey *k = *privkey;
    for (OctetString *o : {&k->modulus, &k->pubex, &k->privex, &k->prime1, &k->prime2, &k->exp1, &k->exp2, &k->coeff})
        aux_free2_OctetString(o);
    mem_free(k);
    *privkey = nullptr;
}

void aux_free_SET_OF_IssuedCertificate(SET_OF_IssuedCertificate **isscertset)
{
    if (!isscertset)
        return;
    SET_OF_IssuedCertificate *s = *isscertset;
    while (s && mem_owned(s)) {
        SET_OF_IssuedCertificate *next = s->next;
        if (s->element) {
            aux_free_OctetString(&s->element->serial);
            mem_free(s->element->date_of_issue);
            mem_free(s->element);
        }
        mem_free(s);
        s = next;
    }
    *isscertset = nullptr;
}

void aux_free_PSEToc(PSEToc **toc)
{
    if (!toc || !*toc)
        return;
    if (!mem_owned(*toc)) {
        *toc = nullptr;
        return;
    }
    PSEToc *t = *toc;
    mem_free(t->owner);
    mem_free(t->create);
    mem_free(t->update);
    PSEObjects *o = t->obj;
    while (o) {
        PSEObjects *next = o->next;
        mem_free(o->name);
        mem_free(o->create);
        mem_free(o->update);
        aux_free_ObjId(&o->objType);
        mem_free(o);
        o = next;
    }
    mem_free(t);
    *toc = nullptr;
}

} // extern "C"
