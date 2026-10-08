// Personal Security Environment: a PIN protected key store file.
//
//   CompatPSE ::= SEQUENCE {
//       format     UTF8String "SECUDE-compat PSE",
//       version    INTEGER (1),
//       kdf        SEQUENCE { pbkdf2 OID, salt OCTET STRING, iterations INTEGER, hmacWithSHA256 OID },
//       cipher     SEQUENCE { aes256-GCM OID, nonce OCTET STRING },
//       ciphertext OCTET STRING }   -- AES-256-GCM(PSEContents) || tag, AAD = DER of the fields above
//   PSEContents ::= SEQUENCE { flags INTEGER, created Time, objects SEQUENCE OF PSEObject }
//   PSEObject   ::= SEQUENCE { name UTF8String, type OID, created Time, updated Time, value OCTET STRING }
#include "der.hpp"

#include <map>
#include <mutex>

namespace compat {

namespace {

const char kFormat[] = "SECUDE-compat PSE";
const Oid kAesGcm{2, 16, 840, 1, 101, 3, 4, 1, 46};
constexpr int kIterations = 100000;

} // namespace

struct PseStore {
    std::string path;
    bool onekeypaironly = true;
    std::string created;
    std::vector<PseObject> objects;
    Bytes salt;
    int iterations = kIterations;
    Bytes key;

    void derive(const char *pin)
    {
        key.assign(32, 0);
        const char *p = pin ? pin : "";
        if (!PKCS5_PBKDF2_HMAC(p, int(std::strlen(p)), salt.data(), int(salt.size()), iterations, EVP_sha256(),
                               int(key.size()), key.data()))
            fail(EINTERNAL, "key derivation failed");
    }

    Bytes header() const
    {
        return der::concat({der::string(der::UTF8String, kFormat), der::integer(1),
                            der::seq({der::oid(oids::pbkdf2), der::octet_string(salt), der::integer(iterations),
                                      der::oid(oids::hmacSHA256)})});
    }

    Bytes contents() const
    {
        Bytes objs;
        for (const auto &o : objects)
            der::append(objs, der::seq({der::string(der::UTF8String, o.name), der::oid(o.type), der::time(o.created),
                                         der::time(o.updated), der::octet_string(o.value)}));
        return der::seq({der::integer(onekeypaironly ? 1 : 0), der::time(created), der::tlv(der::SEQUENCE, objs)});
    }

    void save() const
    {
        Bytes plain = contents();
        Bytes nonce = random_bytes(12);
        Bytes head = header();
        Bytes cipher_params = der::seq({der::oid(kAesGcm), der::octet_string(nonce)});
        Bytes aad = der::concat({head, cipher_params});
        Bytes ct = aes_gcm(true, nonce, aad, plain);
        write_file_atomic(path, der::tlv(der::SEQUENCE, der::concat({aad, der::octet_string(ct)})));
    }

    void load(const char *pin)
    {
        bool exists = false;
        Bytes file = read_file(path, exists);
        if (!exists)
            fail(EPSENOTEXISTING, "PSE " + path + " does not exist");
        Bytes nonce, ct, aad;
        try {
            der::Node top = der::parse_one(file);
            if (top.tag != der::SEQUENCE)
                fail(EPSEFILE);
            der::Reader r(top);
            der::Node fmt = r.next(der::UTF8String);
            if (der::get_string(fmt) != kFormat)
                fail(EPSEFILE);
            der::Node ver = r.next(der::INTEGER);
            if (der::get_integer(ver) != 1)
                fail(EPSEFILESTRUCTURE, "unsupported PSE version");
            der::Node kdf = r.next(der::SEQUENCE);
            der::Reader kr(kdf);
            if (der::get_oid(kr.next()) != oids::pbkdf2)
                fail(EPSEFILESTRUCTURE, "unsupported key derivation");
            salt = kr.next(der::OCTET_STRING).value();
            iterations = int(der::get_integer(kr.next(der::INTEGER)));
            if (der::get_oid(kr.next()) != oids::hmacSHA256 || iterations < 1)
                fail(EPSEFILESTRUCTURE, "unsupported key derivation");
            der::Node cp = r.next(der::SEQUENCE);
            der::Reader cr(cp);
            if (der::get_oid(cr.next()) != kAesGcm)
                fail(EPSEFILESTRUCTURE, "unsupported PSE encryption");
            nonce = cr.next(der::OCTET_STRING).value();
            ct = r.next(der::OCTET_STRING).value();
            r.expect_end();
            aad.assign(fmt.hdr, cp.hdr + cp.total);
        } catch (const Error &e) {
            if (e.code == EDECODE)
                fail(EPSEFILE, path + " is not a PSE of this implementation");
            throw;
        }
        derive(pin);
        Bytes plain = aes_gcm(false, nonce, aad, ct);
        der::Node c = der::parse_one(plain);
        der::Reader r(c);
        onekeypaironly = der::get_integer(r.next(der::INTEGER)) & 1;
        created = der::get_time(r.next());
        der::Reader objs(r.next(der::SEQUENCE));
        objects.clear();
        while (!objs.empty()) {
            der::Reader o(objs.next(der::SEQUENCE));
            PseObject po;
            po.name = der::get_string(o.next(der::UTF8String));
            po.type = der::get_oid(o.next());
            po.created = der::get_time(o.next());
            po.updated = der::get_time(o.next());
            po.value = o.next(der::OCTET_STRING).value();
            objects.push_back(std::move(po));
        }
    }

private:
    Bytes aes_gcm(bool encrypt, const Bytes &nonce, const Bytes &aad, const Bytes &in) const
    {
        EVP_CIPHER *cipher = fetch_cipher("AES-256-GCM");
        if (!cipher)
            fail(EINTERNAL, "AES-256-GCM not available");
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        Bytes out(in.size() + 16);
        int len = 0, fin = 0;
        bool ok = ctx && EVP_CipherInit_ex2(ctx, cipher, nullptr, nullptr, encrypt, nullptr);
        size_t data_len = in.size();
        if (!encrypt) {
            if (in.size() < 16)
                ok = false;
            else
                data_len -= 16;
        }
        if (ok)
            ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, int(nonce.size()), nullptr) &&
                 EVP_CipherInit_ex2(ctx, nullptr, key.data(), nonce.data(), encrypt, nullptr) &&
                 EVP_CipherUpdate(ctx, nullptr, &len, aad.data(), int(aad.size())) &&
                 EVP_CipherUpdate(ctx, out.data(), &len, in.data(), int(data_len));
        if (ok && !encrypt)
            ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<uint8_t *>(in.data() + data_len));
        if (ok)
            ok = EVP_CipherFinal_ex(ctx, out.data() + len, &fin);
        if (ok && encrypt)
            ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, out.data() + len + fin);
        EVP_CIPHER_CTX_free(ctx);
        EVP_CIPHER_free(cipher);
        if (!ok) {
            if (!encrypt)
                fail(EPIN, "wrong PIN or damaged PSE");
            fail(EINTERNAL, "PSE encryption failed");
        }
        out.resize(size_t(len + fin) + (encrypt ? 16 : 0));
        return out;
    }
};

namespace {

struct HandleData {
    std::shared_ptr<PseStore> store;
};

struct SelData {
    std::shared_ptr<PseStore> store;
    PSE owner = nullptr; // nullptr: opened by sec_open/sec_create
};

struct Registry {
    std::mutex lock;
    std::map<PSE, HandleData> handles;
    std::map<PSESel *, SelData> sels;
};

Registry &reg()
{
    static Registry *r = new Registry;
    return *r;
}

std::shared_ptr<PseStore> open_store(const char *path, const char *pin)
{
    if (!path || !*path)
        fail(EINVALID, "missing PSE name");
    auto s = std::make_shared<PseStore>();
    s->path = path;
    s->load(pin);
    return s;
}

std::shared_ptr<PseStore> create_store(const char *path, const char *pin, bool onekeypaironly)
{
    if (!path || !*path)
        fail(EINVALID, "missing PSE name");
    bool exists = false;
    read_file(path, exists);
    if (exists)
        fail(EPSEALREADYEXISTING, std::string("PSE ") + path + " already exists");
    auto s = std::make_shared<PseStore>();
    s->path = path;
    s->onekeypaironly = onekeypaironly;
    s->created = utc_now();
    s->salt = random_bytes(16);
    s->derive(pin);
    s->save();
    return s;
}

PSE new_handle(const std::shared_ptr<PseStore> &store, const char *psename, const char *cadir)
{
    AF_ctx *ctx = mem_new<AF_ctx>();
    AF_options defaults = AF_DEF_OPTIONS;
    ctx->options = defaults;
    ctx->onekeypaironly = store->onekeypaironly;
    ctx->pse_name = mem_strdup(psename);
    ctx->ca_dir = mem_strdup(cadir);
    Registry &r = reg();
    std::lock_guard<std::mutex> g(r.lock);
    r.handles[ctx].store = store;
    return ctx;
}

PSESel *new_sel(const std::shared_ptr<PseStore> &store, PSE owner)
{
    PSESel *sel = mem_new<PSESel>();
    Registry &r = reg();
    std::lock_guard<std::mutex> g(r.lock);
    r.sels[sel] = SelData{store, owner};
    return sel;
}

void destroy_sel(PSESel *sel)
{
    mem_free(sel->object);
    aux_free_ObjId(&sel->object_type);
    mem_free(sel);
}

bool is_static_objid(const ObjId *o)
{
    return o && !mem_owned(o);
}

std::string object_name_for(const char *name, const ObjId *type)
{
    if (name && *name)
        return name;
    if (const char *n = pse_object_name(type))
        return n;
    fail(EOBJNAME, "missing PSE object name");
}

Oid object_type_for(const std::string &name, const ObjId *type)
{
    if (type && type->oid_nelem > 0)
        return oid_of(type);
    if (const ObjId *o = pse_object_oid(name.c_str()))
        return oid_of(o);
    return oid_of(&Uid_oid);
}

bool type_in(const Oid &t, std::initializer_list<const ObjId *> list)
{
    for (const ObjId *o : list)
        if (t == oid_of(o))
            return true;
    return false;
}

bool is_cert_type(const Oid &t) { return type_in(t, {&Cert_oid, &SignCert_oid, &EncCert_oid, &AuthCert_oid}); }

bool is_key_type(const Oid &t)
{
    return type_in(t, {&SKnew_oid, &SKold_oid, &SignSK_oid, &DecSKnew_oid, &DecSKold_oid, &AuthSK_oid});
}

void *decode_object(const PseObject &o)
{
    if (is_cert_type(o.type))
        return dec_certificate(o.value);
    if (is_key_type(o.type))
        return dec_keyinfo(o.value);
    if (o.type == oid_of(&PKRoot_oid))
        return dec_pkroot(o.value);
    if (o.type == oid_of(&FCPath_oid))
        return dec_fcpath(o.value);
    if (o.type == oid_of(&Name_oid))
        return dec_dname(o.value);
    return new_ostr(o.value);
}

Bytes encode_object(const Oid &type, void *opaque)
{
    if (!opaque)
        fail(EINVALID, "missing PSE object value");
    if (is_cert_type(type))
        return enc_certificate(static_cast<Certificate *>(opaque));
    if (is_key_type(type))
        return enc_keyinfo(static_cast<KeyInfo *>(opaque));
    if (type == oid_of(&PKRoot_oid))
        return enc_pkroot(static_cast<PKRoot *>(opaque));
    if (type == oid_of(&FCPath_oid))
        return enc_fcpath(static_cast<FCPath *>(opaque));
    if (type == oid_of(&Name_oid))
        return enc_dname(static_cast<DName *>(opaque));
    return bytes_of(static_cast<OctetString *>(opaque));
}

const PseObject &require(const std::shared_ptr<PseStore> &s, const std::string &name)
{
    const PseObject *o = store_find(s, name);
    if (!o)
        fail(EPSEOBJECTNOTEXISTING, "PSE object " + name + " does not exist");
    return *o;
}

Bytes toc_der(const PseStore &s)
{
    Bytes objs;
    for (const auto &o : s.objects)
        der::append(objs, der::seq({der::string(der::UTF8String, o.name), der::time(o.created), der::time(o.updated),
                                     der::integer(long(o.value.size())), der::integer(0), der::oid(o.type)}));
    std::string update = s.created;
    for (const auto &o : s.objects)
        if (o.updated > update)
            update = o.updated;
    return der::seq({der::string(der::UTF8String, s.path), der::time(s.created), der::time(update), der::integer(0),
                     der::tlv(der::SEQUENCE, objs)});
}

} // namespace

std::shared_ptr<PseStore> store_of(PSE pse)
{
    Registry &r = reg();
    std::lock_guard<std::mutex> g(r.lock);
    auto it = r.handles.find(pse);
    if (it == r.handles.end())
        fail(EINVALID, "invalid PSE handle");
    return it->second.store;
}

std::shared_ptr<PseStore> store_of(PSESel *sel)
{
    Registry &r = reg();
    std::lock_guard<std::mutex> g(r.lock);
    auto it = r.sels.find(sel);
    if (it == r.sels.end())
        fail(EINVALID, "invalid PSE selector");
    return it->second.store;
}

const PseObject *store_find(const std::shared_ptr<PseStore> &s, const std::string &name)
{
    for (const auto &o : s->objects)
        if (o.name == name)
            return &o;
    return nullptr;
}

void store_put(const std::shared_ptr<PseStore> &s, const std::string &name, const Oid &type, const Bytes &value)
{
    std::string now = utc_now();
    for (auto &o : s->objects)
        if (o.name == name) {
            o.type = type;
            o.value = value;
            o.updated = now;
            s->save();
            return;
        }
    s->objects.push_back(PseObject{name, type, now, now, value});
    s->save();
}

bool store_onekeypaironly(const std::shared_ptr<PseStore> &s) { return s->onekeypaironly; }

const char *pse_cadir(PSE pse)
{
    store_of(pse);
    return pse->ca_dir;
}

static std::string pick(const std::shared_ptr<PseStore> &s, const char *preferred, const char *fallback)
{
    if (!s->onekeypaironly && store_find(s, preferred))
        return preferred;
    return fallback;
}

std::string private_key_object(PSE pse, KeyType type)
{
    auto s = store_of(pse);
    return pick(s, type == SIGNATURE ? SignSK_name : DecSKnew_name, SKnew_name);
}

std::string certificate_object(PSE pse, KeyType type)
{
    auto s = store_of(pse);
    return pick(s, type == SIGNATURE ? SignCert_name : EncCert_name, Cert_name);
}

KeyInfo *read_private_key(PSE pse, KeyType type)
{
    auto s = store_of(pse);
    return dec_keyinfo(require(s, private_key_object(pse, type)).value);
}

Bytes rsa_exponent_hint(PSE pse, const KeyInfo *priv)
{
    if (!keyinfo_is_rsa(priv))
        return {};
    auto s = store_of(pse);
    for (const char *name : {Cert_name, SignCert_name, EncCert_name}) {
        const PseObject *o = store_find(s, name);
        if (!o)
            continue;
        try {
            Certificate *c = dec_certificate(o->value);
            Bytes e;
            try {
                RsaPublic pub = rsa_public_from_keyinfo(c->tbs->subjectPK);
                RsaPrivate k = rsa_private_from_keyinfo(priv, pub.e);
                if (k.n == pub.n)
                    e = pub.e;
            } catch (const Error &) {
            }
            free_certificate(c);
            if (!e.empty())
                return e;
        } catch (const Error &) {
        }
    }
    return {};
}

} // namespace compat

using namespace compat;

extern "C" {

PSE af_create(char *psename, char *cadir, char *pin, SC_DATA *sc_data, Boolean onekeypaironly)
{
    return guarded<PSE>("af_create", nullptr, [&] {
        return new_handle(create_store(psename, pin, onekeypaironly != 0), psename, cadir);
    });
}

PSE af_open(char *psename, char *cadir, char *pin, SC_DATA *sc_data)
{
    return guarded<PSE>("af_open", nullptr, [&] { return new_handle(open_store(psename, pin), psename, cadir); });
}

RC af_close(PSE pse_handle)
{
    return guarded<RC>("af_close", -1, [&] {
        Registry &r = reg();
        std::vector<PSESel *> sels;
        {
            std::lock_guard<std::mutex> g(r.lock);
            if (!r.handles.erase(pse_handle))
                fail(EINVALID, "invalid PSE handle");
            for (auto it = r.sels.begin(); it != r.sels.end();) {
                if (it->second.owner == pse_handle) {
                    sels.push_back(it->first);
                    it = r.sels.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (PSESel *s : sels)
            destroy_sel(s);
        mem_free(pse_handle->pse_name);
        mem_free(pse_handle->ca_dir);
        mem_free(pse_handle);
        return 0;
    });
}

PSESel *af_get_PSESel(PSE pse_hdl, ObjId *af_object)
{
    return guarded<PSESel *>("af_get_PSESel", nullptr, [&] {
        PSESel *sel = new_sel(store_of(pse_hdl), pse_hdl);
        if (const char *name = pse_object_name(af_object)) {
            sel->object = mem_strdup(name);
            sel->object_type = copy_objid(af_object);
        }
        return sel;
    });
}

RC af_get_options(PSE pse, AF_options *options)
{
    return guarded<RC>("af_get_options", -1, [&] {
        store_of(pse);
        if (!options)
            fail(EINVALID, "missing options");
        *options = pse->options;
        return 0;
    });
}

PSESel *sec_create(char *psename, char *pin, SC_DATA *sc_data, Boolean onekeypaironly)
{
    return guarded<PSESel *>("sec_create", nullptr, [&] { return new_sel(create_store(psename, pin, onekeypaironly != 0), nullptr); });
}

PSESel *sec_open(char *pse_name, char *pin, SC_DATA *sc_data)
{
    return guarded<PSESel *>("sec_open", nullptr, [&] { return new_sel(open_store(pse_name, pin), nullptr); });
}

static RC release_sel(PSESel *pse_sel)
{
    Registry &r = reg();
    {
        std::lock_guard<std::mutex> g(r.lock);
        if (!r.sels.erase(pse_sel))
            return -1;
    }
    destroy_sel(pse_sel);
    return 0;
}

RC sec_close(PSESel *pse_sel)
{
    if (release_sel(pse_sel) < 0) {
        push_error(EINVALID, "sec_close", "invalid PSE selector");
        return -1;
    }
    return 0;
}

void aux_free_PSESel(PSESel **pse_sel)
{
    if (!pse_sel || !*pse_sel)
        return;
    if (release_sel(*pse_sel) < 0 && mem_owned(*pse_sel))
        destroy_sel(*pse_sel);
    *pse_sel = nullptr;
}

int sec_onekeypaironly(PSESel *pse_sel)
{
    return guarded<int>("sec_onekeypaironly", 0, [&] { return store_onekeypaironly(store_of(pse_sel)) ? TRUE : FALSE; });
}

int sec_write_PSE(PSESel *pse_sel, OctetString *value)
{
    return guarded<int>("sec_write_PSE", -1, [&] {
        auto s = store_of(pse_sel);
        std::string name = object_name_for(pse_sel->object, pse_sel->object_type);
        if (!value)
            fail(EINVALID, "missing value");
        store_put(s, name, object_type_for(name, pse_sel->object_type), bytes_of(value));
        return 0;
    });
}

RC sec_read_PSE(PSESel *pse_sel, OctetString *value)
{
    return guarded<RC>("sec_read_PSE", -1, [&] {
        auto s = store_of(pse_sel);
        std::string name = object_name_for(pse_sel->object, pse_sel->object_type);
        const PseObject &o = require(s, name);
        if (pse_sel->object_type && pse_sel->object_type->oid_nelem > 0 && oid_of(pse_sel->object_type) != o.type &&
            !oid_equal(pse_sel->object_type, &Uid_oid))
            fail(EOBJ, "PSE object " + name + " has a different type");
        if (!value)
            fail(EINVALID, "missing value");
        fill_ostr(value, o.value);
        return 0;
    });
}

RC sec_write(PSESel *pse_sel, OctetString *content)
{
    return guarded<RC>("sec_write", -1, [&] {
        auto s = store_of(pse_sel);
        std::string name = object_name_for(pse_sel->object, pse_sel->object_type);
        if (!content)
            fail(EINVALID, "missing content");
        const PseObject *existing = store_find(s, name);
        Oid type = existing && !(pse_sel->object_type && pse_sel->object_type->oid_nelem > 0)
                       ? existing->type
                       : object_type_for(name, pse_sel->object_type);
        store_put(s, name, type, bytes_of(content));
        return 0;
    });
}

RC sec_read(PSESel *pse_sel, OctetString *content)
{
    return guarded<RC>("sec_read", -1, [&] {
        auto s = store_of(pse_sel);
        if (!content)
            fail(EINVALID, "missing content");
        if (pse_sel->object && !std::strcmp(pse_sel->object, Toc_name)) {
            fill_ostr(content, toc_der(*s));
            return 0;
        }
        fill_ostr(content, require(s, object_name_for(pse_sel->object, pse_sel->object_type)).value);
        return 0;
    });
}

PSEToc *d_PSEToc(OctetString *asn1_string)
{
    return guarded<PSEToc *>("d_PSEToc", nullptr, [&] {
        Bytes in = bytes_of(asn1_string);
        der::Node top = der::parse_one(in);
        if (top.tag != der::SEQUENCE)
            fail(EDECODE, "PSE table of contents expected");
        PSEToc *t = mem_new<PSEToc>();
        try {
            der::Reader r(top);
            t->owner = mem_strdup(der::get_string(r.next(der::UTF8String)));
            t->create = mem_strdup(der::get_time(r.next()));
            t->update = mem_strdup(der::get_time(r.next()));
            t->status = unsigned(der::get_integer(r.next(der::INTEGER)));
            der::Reader objs(r.next(der::SEQUENCE));
            PSEObjects **tail = &t->obj;
            while (!objs.empty()) {
                der::Reader o(objs.next(der::SEQUENCE));
                PSEObjects *po = mem_new<PSEObjects>();
                *tail = po;
                tail = &po->next;
                po->name = mem_strdup(der::get_string(o.next(der::UTF8String)));
                po->create = mem_strdup(der::get_time(o.next()));
                po->update = mem_strdup(der::get_time(o.next()));
                po->noOctets = int(der::get_integer(o.next(der::INTEGER)));
                po->status = unsigned(der::get_integer(o.next(der::INTEGER)));
                po->objType = new_objid(der::get_oid(o.next()));
            }
            return t;
        } catch (...) {
            aux_free_PSEToc(&t);
            throw;
        }
    });
}

void *af_pse_get(PSE pse_handle, char *objname, ObjId *objtype)
{
    return guarded<void *>("af_pse_get", nullptr, [&] {
        auto s = store_of(pse_handle);
        if (!objname)
            fail(EOBJNAME, "missing PSE object name");
        const PseObject &o = require(s, objname);
        void *result = decode_object(o);
        if (objtype && !is_static_objid(objtype))
            fill_objid(objtype, o.type);
        return result;
    });
}

RC af_pse_update(PSE pse_handle, char *objname, void *opaque, ObjId *opaque_type)
{
    return guarded<RC>("af_pse_update", -1, [&] {
        auto s = store_of(pse_handle);
        std::string name = object_name_for(objname, opaque_type);
        Oid type = object_type_for(name, opaque_type);
        store_put(s, name, type, encode_object(type, opaque));
        return 0;
    });
}

DName *af_pse_get_Name(PSE pse_handle)
{
    return guarded<DName *>("af_pse_get_Name", nullptr, [&]() -> DName * {
        auto s = store_of(pse_handle);
        if (const PseObject *o = store_find(s, "Name"))
            return dec_dname(o->value);
        const PseObject &c = require(s, certificate_object(pse_handle, SIGNATURE));
        Certificate *cert = dec_certificate(c.value);
        DName *d = copy_dname(cert->tbs->subject);
        free_certificate(cert);
        return d;
    });
}

SerialNumber *af_pse_get_SerialNumber(PSE pse_handle)
{
    return guarded<SerialNumber *>("af_pse_get_SerialNumber", nullptr, [&] {
        return new_ostr(require(store_of(pse_handle), SerialNumber_name).value);
    });
}

RC af_pse_update_SerialNumber(PSE pse_handle, SerialNumber *serial)
{
    return guarded<RC>("af_pse_update_SerialNumber", -1, [&] {
        if (!serial || !serial->noctets)
            fail(EINVALID, "missing serial number");
        store_put(store_of(pse_handle), SerialNumber_name, oid_of(&SerialNumber_oid), bytes_of(serial));
        return 0;
    });
}

PKRoot *af_pse_get_PKRoot(PSE pse_handle)
{
    return guarded<PKRoot *>("af_pse_get_PKRoot", nullptr, [&] {
        return dec_pkroot(require(store_of(pse_handle), PKRoot_name).value);
    });
}

int af_pse_update_PKRoot(PSE pse_handle, PKRoot *pkroot)
{
    return guarded<int>("af_pse_update_PKRoot", -1, [&] {
        store_put(store_of(pse_handle), PKRoot_name, oid_of(&PKRoot_oid), enc_pkroot(pkroot));
        return 0;
    });
}

FCPath *af_pse_get_FCPath(PSE pse_handle, DName *name)
{
    return guarded<FCPath *>("af_pse_get_FCPath", nullptr, [&] {
        return dec_fcpath(require(store_of(pse_handle), FCPath_name).value);
    });
}

CrlSet *af_pse_get_CrlSet(PSE pse_handle)
{
    return guarded<CrlSet *>("af_pse_get_CrlSet", nullptr, [&]() -> CrlSet * {
        store_of(pse_handle);
        fail(ENOCRL, "revocation lists are not supported by this implementation");
    });
}

} // extern "C"
