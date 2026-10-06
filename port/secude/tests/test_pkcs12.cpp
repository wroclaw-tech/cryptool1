#include "bn.hpp"
#include "testing.hpp"

#include <openssl/pkcs12.h>
#include <openssl/x509.h>

using namespace compat;

namespace {

std::string root() { return testing::scratch_dir() + "/p12"; }

// CrypTool's PKCS12_encode (SecudeTools.cpp)
OctetString *export_pse(PSE pse, OctetString *PIN, int encrypt_priv_key)
{
    P12_Safe P12safe;
    std::memset(&P12safe, 0, sizeof(P12safe));
    P12safe.version = 3;
    P12safe.iterationCount = 2048;
    P12safe.alg_oid.mac = aux_cpy_ObjId(&sha1_oid);
    P12safe.alg_oid.enc = aux_cpy_ObjId(&pbeWithSHA1And40BitRC2CBC_oid);
    P12safe.alg_oid.espvk = aux_cpy_ObjId(&pbeWithSHA1AndDES3xCBC_oid);
    CHECK(sec_onekeypaironly(af_get_PSESel(pse, nullptr)));
    ObjId oid;
    Certificate *cert = (Certificate *)af_pse_get(pse, const_cast<char *>("Cert"), &oid);
    KeyInfo *key = (KeyInfo *)af_pse_get(pse, const_cast<char *>("SKnew"), &oid);
    CHECK(cert && key);
    char oarr[4] = {0x01, 0x00, 0x00, 0x00};
    OctetString ostr{4, oarr};
    OctetString *lkid = aux_cpy_OctetString(&ostr);

    P12_Bag *keybag = (P12_Bag *)aux_malloc(sizeof(P12_Bag));
    std::memset(keybag, 0, sizeof(P12_Bag));
    keybag->int_encryption = encrypt_priv_key;
    keybag->type = P12_bc_key;
    keybag->friendlyName = aux_latin1_to_unicode(const_cast<char *>("name"), FALSE);
    keybag->localKeyID = aux_cpy_OctetString(lkid);
    keybag->content.key = aux_create_PrivateKeyInfo(key, cert, nullptr);
    CHECK(keybag->content.key != nullptr);
    P12_Bag *certbag = (P12_Bag *)aux_malloc(sizeof(P12_Bag));
    std::memset(certbag, 0, sizeof(P12_Bag));
    certbag->int_encryption = TRUE;
    certbag->type = P12_bc_cert;
    certbag->localKeyID = lkid;
    certbag->content.cert = cert;
    keybag->next = certbag;
    P12safe.bags = keybag;
    aux_free_KeyInfo(&key);

    PKRoot *pkroot = af_pse_get_PKRoot(pse);
    if (pkroot) {
        P12_Bag *rootbag = (P12_Bag *)aux_malloc(sizeof(P12_Bag));
        std::memset(rootbag, 0, sizeof(P12_Bag));
        rootbag->int_encryption = TRUE;
        rootbag->type = P12_bc_cert;
        rootbag->content.cert = af_PKRoot2Protocert(pkroot);
        certbag->next = rootbag;
        aux_free_PKRoot(&pkroot);
    }
    OctetString *P12container = aux_alloc_OctetString();
    CHECK_EQ(pkcs12_encode(&P12safe, PIN, P12container), 0);
    return P12container;
}

} // namespace

TEST(pkcs12_export_import_round_trip)
{
    std::string dir = root();
    CHECK_EQ(secude_compat_create_sample_keystore(dir.c_str(), 0), 0);
    std::string bob = dir + "/PSE/[SideChannelAttack][Bob][RSA-512][1152179494][PIN=1234].pse";
    PSE pse = af_open(const_cast<char *>(bob.c_str()), nullptr, const_cast<char *>("1234"), nullptr);
    CHECK(pse != nullptr);
    OctetString *pw = aux_latin1_to_unicode(const_cast<char *>("secret"), TRUE);
    OctetString *p12 = export_pse(pse, pw, TRUE);
    CHECK(p12 && p12->noctets > 500);
    char *dump = sdumpasn(p12, 0, 0);
    CHECK(std::string(dump).find("FATAL") == std::string::npos);
    aux_free_String(&dump);

    // CrypTool's PKCS12_import into a new PSE
    P12_Safe safe;
    std::memset(&safe, 0, sizeof safe);
    Boolean verified = 0;
    CHECK_EQ(pkcs12_decode(p12, pw, &safe, &verified), 0);
    CHECK(verified);
    CHECK_EQ(safe.iterationCount, 2048);
    CHECK_EQ(aux_cmp_ObjId(safe.alg_oid.enc, &pbeWithSHA1And40BitRC2CBC_oid), 0);
    int keys = 0, certs = 0;
    PrivateKeyInfo *pki = nullptr;
    for (P12_Bag *b = safe.bags; b; b = b->next) {
        if (b->type == P12_bc_key) {
            ++keys;
            pki = b->content.key;
            CHECK(b->localKeyID && b->friendlyName);
        } else if (b->type == P12_bc_cert) {
            ++certs;
        }
    }
    CHECK_EQ(keys, 1);
    CHECK_EQ(certs, 2);
    CHECK_EQ(aux_ObjId2AlgEnc(pki->privateKeyAlgorithm->objid), SECUDE_ALG_RSA);
    RSAPrivateKey *rsa = d_RSAPrivateKey(pki->privateKey);
    CHECK(rsa && rsa->pubex.noctets == 3);
    KeyInfo *orig = (KeyInfo *)af_pse_get(pse, const_cast<char *>("SKnew"), nullptr);
    KeyBits *kb = d_KeyBits(&orig->subjectkey);
    CHECK(bytes_of(&kb->part1) == bytes_of(&rsa->prime1));
    CHECK(bytes_of(&kb->part2) == bytes_of(&rsa->prime2));
    aux_free_KeyBits(&kb);
    aux_free_KeyInfo(&orig);
    aux_free_RSAPrivateKey(&rsa);

    OctetString *wrong = aux_latin1_to_unicode(const_cast<char *>("wrong"), TRUE);
    P12_Safe other;
    std::memset(&other, 0, sizeof other);
    CHECK_EQ(pkcs12_decode(p12, wrong, &other, &verified), -1);
    CHECK(!verified);
    aux_free_error();

    // the same file is readable by OpenSSL (MAC and shrouded key do not need the legacy provider)
    const unsigned char *p = reinterpret_cast<const unsigned char *>(p12->octets);
    PKCS12 *ossl = d2i_PKCS12(nullptr, &p, long(p12->noctets));
    CHECK(ossl != nullptr);
    CHECK_EQ(PKCS12_verify_mac(ossl, "secret", -1), 1);
    PKCS12_free(ossl);

    aux_free_OctetString(&wrong);
    aux_free_OctetString(&p12);
    aux_free_OctetString(&pw);
    af_close(pse);
}

TEST(pkcs12_import_openssl_default_file)
{
    // RSA key and certificate created by OpenSSL 3 with its defaults (PBES2, AES-256, SHA-256 MAC)
    EVP_PKEY *pkey = EVP_RSA_gen(1024);
    X509 *x = X509_new();
    X509_set_version(x, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x), 7);
    X509_gmtime_adj(X509_getm_notBefore(x), 0);
    X509_gmtime_adj(X509_getm_notAfter(x), 3600);
    X509_set_pubkey(x, pkey);
    X509_NAME *name = X509_get_subject_name(x);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("OpenSSL user"), -1, -1, 0);
    X509_set_issuer_name(x, name);
    X509_sign(x, pkey, EVP_sha256());
    PKCS12 *p12 = PKCS12_create("pass", "friendly", pkey, x, nullptr, 0, 0, 0, 0, 0);
    CHECK(p12 != nullptr);
    unsigned char *der = nullptr;
    int len = i2d_PKCS12(p12, &der);
    OctetString in{sec_uint4(len), reinterpret_cast<char *>(der)};
    OctetString *pw = aux_latin1_to_unicode(const_cast<char *>("pass"), TRUE);
    P12_Safe safe;
    std::memset(&safe, 0, sizeof safe);
    Boolean verified = 0;
    CHECK_EQ(pkcs12_decode(&in, pw, &safe, &verified), 0);
    CHECK(verified);
    int keys = 0, certs = 0;
    for (P12_Bag *b = safe.bags; b; b = b->next) {
        if (b->type == P12_bc_key) {
            ++keys;
            RSAPrivateKey *rsa = d_RSAPrivateKey(b->content.key->privateKey);
            CHECK(rsa != nullptr);
            aux_free_RSAPrivateKey(&rsa);
        } else if (b->type == P12_bc_cert) {
            ++certs;
            CHECK_EQ(std::string(aux_DName2Name(b->content.cert->tbs->subject)), std::string("CN=OpenSSL user"));
        }
    }
    CHECK_EQ(keys, 1);
    CHECK_EQ(certs, 1);
    aux_free_OctetString(&pw);
    OPENSSL_free(der);
    PKCS12_free(p12);
    X509_free(x);
    EVP_PKEY_free(pkey);
}
