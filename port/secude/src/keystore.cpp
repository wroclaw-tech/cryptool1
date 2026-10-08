// Creation of the CrypTool key store (CA PSE, CA database, sample keys).
#include <openssl/ec.h>
#include <openssl/obj_mac.h>

#include "bn.hpp"
#include "der.hpp"

#include <cerrno>
#include <sys/stat.h>

namespace compat {

namespace {

constexpr long kDay = 24L * 3600;

std::string join(const std::string &dir, const std::string &name)
{
    if (dir.empty() || dir.back() == '/')
        return dir + name;
    return dir + "/" + name;
}

void make_dirs(const std::string &path)
{
    std::string cur;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!cur.empty() && ::mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST)
                fail(EWRITEFILE, "cannot create directory " + cur + ": " + std::strerror(errno));
        }
        if (i < path.size())
            cur += path[i];
    }
}

bool exists(const std::string &path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

// Raises the most recent SECUDE error as exception (for chaining API calls).
[[noreturn]] void rethrow(const char *what)
{
    int code = th_last_error();
    std::string text = th_get_last_error_text();
    fail(code ? code : EINTERNAL, std::string(what) + ": " + text);
}

struct PseGuard {
    PSE pse;
    explicit PseGuard(PSE p) : pse(p) {}
    ~PseGuard()
    {
        if (pse)
            af_close(pse);
    }
    PseGuard(const PseGuard &) = delete;
    PseGuard &operator=(const PseGuard &) = delete;
};

void create_ca(const std::string &pse_path, const std::string &ca_dir, const char *pin, const char *name, int bits,
               int days)
{
    make_dirs(ca_dir);
    std::remove(join(ca_dir, "cadb.der").c_str());
    std::remove(pse_path.c_str());
    PseGuard ca(af_create(const_cast<char *>(pse_path.c_str()), const_cast<char *>(ca_dir.c_str()),
                          const_cast<char *>(pin), nullptr, TRUE));
    if (!ca.pse)
        rethrow("af_create");
    Key key{};
    key.alg = &rsa_aid;
    key.key_size = bits;
    if (af_gen_key(ca.pse, &key, SIGNATURE, TRUE) != 0)
        rethrow("af_gen_key");
    DName *dn = string_to_dname(name);
    Certificate *cert = nullptr;
    try {
        cert = make_ca_certificate(ca.pse, key.key, &sha1WithRsaEncryption_aid, dn, long(days) * kDay);
    } catch (...) {
        free_dname(dn);
        free_keyinfo(key.key);
        throw;
    }
    free_dname(dn);
    free_keyinfo(key.key);
    PKRoot *root = aux_create_PKRoot(cert, nullptr);
    Bytes one{1};
    OctetString serial{1, reinterpret_cast<char *>(one.data())};
    bool ok = root && af_pse_update(ca.pse, const_cast<char *>(Cert_name), cert, Cert_OID) == 0 &&
              af_pse_update_PKRoot(ca.pse, root) == 0 && af_pse_update_SerialNumber(ca.pse, &serial) == 0 &&
              af_cadb_add_Certificate(ca.pse, SIGNATURE, cert) == 0;
    free_pkroot(root);
    free_certificate(cert);
    if (!ok)
        rethrow("CA initialisation");
}

// Mirrors CDlgKeyAsymGeneration::CreateAsymKeys for an RSA key.
void create_rsa_user(const std::string &pse_dir, const std::string &ca_pse, const std::string &ca_dir,
                     const std::string &name, const std::string &first_name, int bits, const std::string &time,
                     const std::string &info, const char *pin)
{
    std::string key_id = "[" + name + "][" + first_name + "][RSA-" + std::to_string(bits) + "][" + time + "][" + info + "]";
    std::string path = join(pse_dir, key_id + ".pse");
    std::remove(path.c_str());
    PseGuard user(af_create(const_cast<char *>(path.c_str()), nullptr, const_cast<char *>(pin), nullptr, TRUE));
    if (!user.pse)
        rethrow("af_create");
    Key key{};
    key.alg = &rsa_aid;
    key.key_size = bits;
    if (af_gen_key(user.pse, &key, SIGNATURE, TRUE) != 0)
        rethrow("af_gen_key");
    std::string dn_text = "CN=" + first_name + " " + name + " [" + time + "], DC=cryptool, DC=org";
    DName *dn = string_to_dname(dn_text.c_str());
    char serial_text[] = "000001";
    OctetString proto_serial{6, serial_text};
    Certificate *proto = af_create_Certificate(user.pse, key.key, &sha1WithRSASignature_aid, const_cast<char *>(SKnew_name),
                                               dn, &proto_serial, nullptr, nullptr, TRUE, nullptr);
    free_dname(dn);
    free_keyinfo(key.key);
    if (!proto)
        rethrow("af_create_Certificate");

    OctetString *ext_value = e_PrintableString(const_cast<char *>(key_id.c_str()));
    v3Extension ext{};
    ObjId ext_oid{};
    fill_objid(&ext_oid, oids::cryptoolPseName);
    ext.extnId = &ext_oid;
    ext.extnDERcode = ext_value;
    SEQUENCE_OF_Extension list{&ext, nullptr};
    CertExtensions cert_ext{};
    cert_ext.nonSupported = &list;
    CertExtensions *own_ext = proto->tbs->extensions;
    proto->tbs->extensions = &cert_ext;

    PseGuard ca(af_open(const_cast<char *>(ca_pse.c_str()), const_cast<char *>(ca_dir.c_str()),
                        const_cast<char *>(SECUDE_COMPAT_CA_PIN), nullptr));
    Certificate *cert = nullptr;
    if (ca.pse) {
        OctetString *serial = sec_random_ostr(8, 1);
        if (serial && af_pse_update_SerialNumber(ca.pse, serial) == 0) {
            Validity v{};
            std::string from = utc_now(), to = utc_now(7305 * kDay);
            v.notbefore = const_cast<char *>(from.c_str());
            v.notafter = const_cast<char *>(to.c_str());
            cert = af_certify(ca.pse, proto, &v, &sha1WithRSASignature_aid, nullptr);
        }
        aux_free_OctetString(&serial);
    }
    proto->tbs->extensions = own_ext;
    free_certificate(proto);
    aux_free_OctetString(&ext_value);
    free_objid_content(&ext_oid);
    if (!cert)
        rethrow("af_certify");
    PKRoot *root = af_pse_get_PKRoot(ca.pse);
    bool ok = root && af_cadb_add_Certificate(ca.pse, SIGNATURE, cert) == 0 && af_pse_update_PKRoot(user.pse, root) == 0 &&
              af_pse_update(user.pse, const_cast<char *>(Cert_name), cert, Cert_OID) == 0;
    free_pkroot(root);
    free_certificate(cert);
    if (!ok)
        rethrow("registering the certificate");
}

std::string hex_number(const BIGNUM *b)
{
    Bytes v = bn_bytes(b);
    std::string h = hex(v.data(), v.size());
    size_t nz = h.find_first_not_of('0');
    return "0X" + (nz == std::string::npos ? std::string("0") : h.substr(nz));
}

// Mirrors CKeyFile::CreateEcKeyFiles: public data as text file, private key in a PSE.
void create_ec_user(const std::string &pse_dir, const std::string &key_id, const char *curve, int nid, const char *pin)
{
    EC_GROUP *group = EC_GROUP_new_by_curve_name_ex(libctx(), nullptr, nid);
    if (!group)
        fail(EINTERNAL, std::string("curve ") + curve + " not available");
    Bn p = bn_new(), a = bn_new(), b = bn_new(), gx = bn_new(), gy = bn_new(), order = bn_new(), cofactor = bn_new();
    Bn d = bn_new(), qx = bn_new(), qy = bn_new();
    EC_POINT *q = EC_POINT_new(group);
    bool ok = q && EC_GROUP_get_curve(group, p.get(), a.get(), b.get(), bn_ctx()) &&
              EC_POINT_get_affine_coordinates(group, EC_GROUP_get0_generator(group), gx.get(), gy.get(), bn_ctx()) &&
              EC_GROUP_get_order(group, order.get(), bn_ctx()) && EC_GROUP_get_cofactor(group, cofactor.get(), bn_ctx());
    while (ok) {
        ok = BN_priv_rand_range(d.get(), order.get());
        if (ok && !BN_is_zero(d.get()))
            break;
    }
    ok = ok && EC_POINT_mul(group, q, d.get(), nullptr, nullptr, bn_ctx()) &&
         EC_POINT_get_affine_coordinates(group, q, qx.get(), qy.get(), bn_ctx());
    EC_POINT_free(q);
    EC_GROUP_free(group);
    if (!ok)
        fail(EINTERNAL, "EC key generation failed");

    std::string text;
    for (const BIGNUM *v : {a.get(), b.get(), p.get(), gx.get(), gy.get(), cofactor.get(), order.get(), qx.get(), qy.get()})
        text += hex_number(v) + "\n";
    std::string pub_path = join(pse_dir, key_id);
    write_file_atomic(pub_path, Bytes(text.begin(), text.end()), 0644);

    std::string pse_path = pub_path + ".pse";
    std::remove(pse_path.c_str());
    PseGuard pse(af_create(const_cast<char *>(pse_path.c_str()), nullptr, const_cast<char *>(pin), nullptr, FALSE));
    if (!pse.pse)
        rethrow("af_create");
    Bytes priv = bn_bytes(d.get());
    OctetString raw{sec_uint4(priv.size()), reinterpret_cast<char *>(priv.data())};
    OctetString *encoded = e_OctetString(&raw);
    int rc = encoded ? af_pse_update(pse.pse, const_cast<char *>("EcPrivKey"), encoded, Uid_OID) : -1;
    aux_free_OctetString(&encoded);
    if (rc != 0)
        rethrow("af_pse_update");
}

} // namespace

} // namespace compat

using namespace compat;

extern "C" {

int secude_compat_create_ca(const char *ca_pse_path, const char *ca_dir, const char *pin, const char *ca_name,
                            int key_bits, int validity_days)
{
    return guarded<int>("secude_compat_create_ca", -1, [&] {
        if (!ca_pse_path || !ca_dir)
            fail(EINVALID, "missing path");
        create_ca(native_path(ca_pse_path), native_path(ca_dir), pin ? pin : SECUDE_COMPAT_CA_PIN, ca_name ? ca_name : SECUDE_COMPAT_CA_NAME,
                  key_bits > 0 ? key_bits : 2048, validity_days > 0 ? validity_days : 7305);
        return 0;
    });
}

int secude_compat_create_sample_keystore(const char *key_store_root, int overwrite)
{
    return guarded<int>("secude_compat_create_sample_keystore", -1, [&] {
        if (!key_store_root)
            fail(EINVALID, "missing key store directory");
        std::string pse_dir = join(native_path(key_store_root), "PSE");
        std::string ca_dir = join(pse_dir, "PSECA");
        std::string ca_pse = join(ca_dir, "capse.cse");
        make_dirs(ca_dir);

        bool new_ca = overwrite || !exists(ca_pse);
        if (new_ca)
            create_ca(ca_pse, ca_dir, SECUDE_COMPAT_CA_PIN, SECUDE_COMPAT_CA_NAME, 2048, 7305);

        const char *rsa_id = "[SideChannelAttack][Bob][RSA-512][1152179494][PIN=1234]";
        if (new_ca || !exists(join(pse_dir, std::string(rsa_id) + ".pse")))
            create_rsa_user(pse_dir, ca_pse, ca_dir, "SideChannelAttack", "Bob", 512, "1152179494", "PIN=1234", "1234");

        const char *ec_id = "[HybridEncryption][Bob][EC-prime239v1][1178702474][PIN=1234]";
        if (overwrite || !exists(join(pse_dir, std::string(ec_id) + ".pse")) || !exists(join(pse_dir, ec_id)))
            create_ec_user(pse_dir, ec_id, "prime239v1", NID_X9_62_prime239v1, "1234");
        return 0;
    });
}

} // extern "C"
