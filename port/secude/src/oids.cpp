#include "internal.hpp"

#include <strings.h>

namespace compat {

namespace oids {
const Oid rsa{2, 5, 8, 1, 1};
const Oid rsaEncryption{1, 2, 840, 113549, 1, 1, 1};
const Oid dsa{1, 3, 14, 3, 2, 12};
const Oid id_dsa{1, 2, 840, 10040, 4, 1};
const Oid md2{1, 2, 840, 113549, 2, 2};
const Oid md4{1, 2, 840, 113549, 2, 4};
const Oid md5{1, 2, 840, 113549, 2, 5};
const Oid sha{1, 3, 14, 3, 2, 18};
const Oid sha1{1, 3, 14, 3, 2, 26};
const Oid ripemd160{1, 3, 36, 3, 2, 1};
const Oid sha256{2, 16, 840, 1, 101, 3, 4, 2, 1};
const Oid sha384{2, 16, 840, 1, 101, 3, 4, 2, 2};
const Oid sha512{2, 16, 840, 1, 101, 3, 4, 2, 3};
const Oid data{1, 2, 840, 113549, 1, 7, 1};
const Oid encryptedData{1, 2, 840, 113549, 1, 7, 6};
const Oid keyBag{1, 2, 840, 113549, 1, 12, 10, 1, 1};
const Oid pkcs8ShroudedKeyBag{1, 2, 840, 113549, 1, 12, 10, 1, 2};
const Oid certBag{1, 2, 840, 113549, 1, 12, 10, 1, 3};
const Oid x509Certificate{1, 2, 840, 113549, 1, 9, 22, 1};
const Oid friendlyName{1, 2, 840, 113549, 1, 9, 20};
const Oid localKeyId{1, 2, 840, 113549, 1, 9, 21};
const Oid pbeSHA1RC4_128{1, 2, 840, 113549, 1, 12, 1, 1};
const Oid pbeSHA1RC4_40{1, 2, 840, 113549, 1, 12, 1, 2};
const Oid pbeSHA1DES3{1, 2, 840, 113549, 1, 12, 1, 3};
const Oid pbeSHA1DES2{1, 2, 840, 113549, 1, 12, 1, 4};
const Oid pbeSHA1RC2_128{1, 2, 840, 113549, 1, 12, 1, 5};
const Oid pbeSHA1RC2_40{1, 2, 840, 113549, 1, 12, 1, 6};
const Oid pbes2{1, 2, 840, 113549, 1, 5, 13};
const Oid pbkdf2{1, 2, 840, 113549, 1, 5, 12};
const Oid hmacSHA1{1, 2, 840, 113549, 2, 7};
const Oid hmacSHA256{1, 2, 840, 113549, 2, 9};
const Oid hmacSHA384{1, 2, 840, 113549, 2, 10};
const Oid hmacSHA512{1, 2, 840, 113549, 2, 11};
const Oid aes128CBC{2, 16, 840, 1, 101, 3, 4, 1, 2};
const Oid aes192CBC{2, 16, 840, 1, 101, 3, 4, 1, 22};
const Oid aes256CBC{2, 16, 840, 1, 101, 3, 4, 1, 42};
const Oid desEDE3CBC{1, 2, 840, 113549, 3, 7};
const Oid subjectKeyIdentifier{2, 5, 29, 14};
const Oid keyUsage{2, 5, 29, 15};
const Oid basicConstraints{2, 5, 29, 19};
const Oid authorityKeyIdentifier{2, 5, 29, 35};
const Oid cryptoolPseName{2, 206, 5, 4, 3, 2};
} // namespace oids

} // namespace compat

using namespace compat;

#define OID_ELEMS(name, ...) static unsigned int name##_elems[] = {__VA_ARGS__}
#define OID_OBJ(name) {int(sizeof(name##_elems) / sizeof(unsigned int)), name##_elems}

// Object identifiers of algorithms
OID_ELEMS(rsa, 2, 5, 8, 1, 1);
OID_ELEMS(rsaEncryption, 1, 2, 840, 113549, 1, 1, 1);
OID_ELEMS(dsa, 1, 3, 14, 3, 2, 12);
OID_ELEMS(md2, 1, 2, 840, 113549, 2, 2);
OID_ELEMS(md4, 1, 2, 840, 113549, 2, 4);
OID_ELEMS(md5, 1, 2, 840, 113549, 2, 5);
OID_ELEMS(sha, 1, 3, 14, 3, 2, 18);
OID_ELEMS(sha1, 1, 3, 14, 3, 2, 26);
OID_ELEMS(ripemd160, 1, 3, 36, 3, 2, 1);
OID_ELEMS(md2WithRsaEncryption, 1, 2, 840, 113549, 1, 1, 2);
OID_ELEMS(md4WithRsaEncryption, 1, 2, 840, 113549, 1, 1, 3);
OID_ELEMS(md5WithRsaEncryption, 1, 2, 840, 113549, 1, 1, 4);
OID_ELEMS(sha1WithRsaEncryption, 1, 2, 840, 113549, 1, 1, 5);
OID_ELEMS(md2WithRSASignature, 1, 3, 14, 3, 2, 24);
OID_ELEMS(shaWithRSASignature, 1, 3, 14, 3, 2, 15);
OID_ELEMS(sha1WithRSASignature, 1, 3, 14, 3, 2, 29);
OID_ELEMS(ripemd160WithRSASignature, 1, 3, 36, 3, 3, 1, 2);
OID_ELEMS(dsaWithSHA, 1, 3, 14, 3, 2, 13);
OID_ELEMS(dsaWithSHA1, 1, 3, 14, 3, 2, 27);
OID_ELEMS(desECB, 1, 3, 14, 3, 2, 6);
OID_ELEMS(desCBC, 1, 3, 14, 3, 2, 7);
OID_ELEMS(desCBC_pad, 1, 3, 36, 3, 1, 1, 2, 1);
OID_ELEMS(desEDE, 1, 3, 14, 3, 2, 17);
OID_ELEMS(desCBC3, 1, 3, 36, 3, 1, 3, 2);
OID_ELEMS(desCBC3_pad, 1, 3, 36, 3, 1, 3, 2, 1);
OID_ELEMS(desEDE3CBC, 1, 2, 840, 113549, 3, 7);
OID_ELEMS(idea, 1, 3, 36, 3, 1, 2);
OID_ELEMS(ideaCBC, 1, 3, 6, 1, 4, 1, 188, 7, 1, 1, 2);
OID_ELEMS(rc2CBC, 1, 2, 840, 113549, 3, 2);
OID_ELEMS(rc4, 1, 2, 840, 113549, 3, 4);
OID_ELEMS(aes128ECB, 2, 16, 840, 1, 101, 3, 4, 1, 1);
OID_ELEMS(aes128CBC, 2, 16, 840, 1, 101, 3, 4, 1, 2);
OID_ELEMS(aes192ECB, 2, 16, 840, 1, 101, 3, 4, 1, 21);
OID_ELEMS(aes192CBC, 2, 16, 840, 1, 101, 3, 4, 1, 22);
OID_ELEMS(aes256ECB, 2, 16, 840, 1, 101, 3, 4, 1, 41);
OID_ELEMS(aes256CBC, 2, 16, 840, 1, 101, 3, 4, 1, 42);
OID_ELEMS(pbeWithSHA1And128BitRC4, 1, 2, 840, 113549, 1, 12, 1, 1);
OID_ELEMS(pbeWithSHA1And40BitRC4, 1, 2, 840, 113549, 1, 12, 1, 2);
OID_ELEMS(pbeWithSHA1AndDES3xCBC, 1, 2, 840, 113549, 1, 12, 1, 3);
OID_ELEMS(pbeWithSHA1AndDES3CBC, 1, 2, 840, 113549, 1, 12, 1, 4);
OID_ELEMS(pbeWithSHA1And128BitRC2CBC, 1, 2, 840, 113549, 1, 12, 1, 5);
OID_ELEMS(pbeWithSHA1And40BitRC2CBC, 1, 2, 840, 113549, 1, 12, 1, 6);

// Object identifiers of PSE object types (private arc 2.206.5.1, see README.md)
OID_ELEMS(SKnew, 2, 206, 5, 1, 1);
OID_ELEMS(SKold, 2, 206, 5, 1, 2);
OID_ELEMS(SignSK, 2, 206, 5, 1, 3);
OID_ELEMS(DecSKnew, 2, 206, 5, 1, 4);
OID_ELEMS(DecSKold, 2, 206, 5, 1, 5);
OID_ELEMS(AuthSK, 2, 206, 5, 1, 6);
OID_ELEMS(Cert, 2, 206, 5, 1, 10);
OID_ELEMS(SignCert, 2, 206, 5, 1, 11);
OID_ELEMS(EncCert, 2, 206, 5, 1, 12);
OID_ELEMS(AuthCert, 2, 206, 5, 1, 13);
OID_ELEMS(PKRoot, 2, 206, 5, 1, 20);
OID_ELEMS(FCPath, 2, 206, 5, 1, 21);
OID_ELEMS(SerialNumber, 2, 206, 5, 1, 30);
OID_ELEMS(Name, 2, 206, 5, 1, 31);
OID_ELEMS(Uid, 2, 206, 5, 1, 40);

static unsigned int rsa_default_keysize = DEF_ASYM_KEYSIZE;
static ObjId md2WithRSASignature_oid_obj = OID_OBJ(md2WithRSASignature);

extern "C" {

ObjId rsa_oid = OID_OBJ(rsa);
ObjId rsaEncryption_oid = OID_OBJ(rsaEncryption);
ObjId dsa_oid = OID_OBJ(dsa);
ObjId md2_oid = OID_OBJ(md2);
ObjId md4_oid = OID_OBJ(md4);
ObjId md5_oid = OID_OBJ(md5);
ObjId sha_oid = OID_OBJ(sha);
ObjId sha1_oid = OID_OBJ(sha1);
ObjId ripemd160_oid = OID_OBJ(ripemd160);
ObjId md2WithRsaEncryption_oid = OID_OBJ(md2WithRsaEncryption);
ObjId md4WithRsaEncryption_oid = OID_OBJ(md4WithRsaEncryption);
ObjId md5WithRsaEncryption_oid = OID_OBJ(md5WithRsaEncryption);
ObjId sha1WithRsaEncryption_oid = OID_OBJ(sha1WithRsaEncryption);
ObjId shaWithRSASignature_oid = OID_OBJ(shaWithRSASignature);
ObjId sha1WithRSASignature_oid = OID_OBJ(sha1WithRSASignature);
ObjId ripemd160WithRSASignature_oid = OID_OBJ(ripemd160WithRSASignature);
ObjId dsaWithSHA_oid = OID_OBJ(dsaWithSHA);
ObjId dsaWithSHA1_oid = OID_OBJ(dsaWithSHA1);
ObjId desECB_oid = OID_OBJ(desECB);
ObjId desCBC_oid = OID_OBJ(desCBC);
ObjId desCBC_pad_oid = OID_OBJ(desCBC_pad);
ObjId desEDE_oid = OID_OBJ(desEDE);
ObjId desCBC3_oid = OID_OBJ(desCBC3);
ObjId desCBC3_pad_oid = OID_OBJ(desCBC3_pad);
ObjId desEDE3CBC_oid = OID_OBJ(desEDE3CBC);
ObjId idea_oid = OID_OBJ(idea);
ObjId ideaCBC_oid = OID_OBJ(ideaCBC);
ObjId rc2CBC_oid = OID_OBJ(rc2CBC);
ObjId rc4_oid = OID_OBJ(rc4);
ObjId aes128ECB_oid = OID_OBJ(aes128ECB);
ObjId aes128CBC_oid = OID_OBJ(aes128CBC);
ObjId aes192ECB_oid = OID_OBJ(aes192ECB);
ObjId aes192CBC_oid = OID_OBJ(aes192CBC);
ObjId aes256ECB_oid = OID_OBJ(aes256ECB);
ObjId aes256CBC_oid = OID_OBJ(aes256CBC);
ObjId pbeWithSHA1And128BitRC4_oid = OID_OBJ(pbeWithSHA1And128BitRC4);
ObjId pbeWithSHA1And40BitRC4_oid = OID_OBJ(pbeWithSHA1And40BitRC4);
ObjId pbeWithSHA1AndDES3xCBC_oid = OID_OBJ(pbeWithSHA1AndDES3xCBC);
ObjId pbeWithSHA1AndDES3CBC_oid = OID_OBJ(pbeWithSHA1AndDES3CBC);
ObjId pbeWithSHA1And128BitRC2CBC_oid = OID_OBJ(pbeWithSHA1And128BitRC2CBC);
ObjId pbeWithSHA1And40BitRC2CBC_oid = OID_OBJ(pbeWithSHA1And40BitRC2CBC);

ObjId SKnew_oid = OID_OBJ(SKnew);
ObjId SKold_oid = OID_OBJ(SKold);
ObjId SignSK_oid = OID_OBJ(SignSK);
ObjId DecSKnew_oid = OID_OBJ(DecSKnew);
ObjId DecSKold_oid = OID_OBJ(DecSKold);
ObjId AuthSK_oid = OID_OBJ(AuthSK);
ObjId Cert_oid = OID_OBJ(Cert);
ObjId SignCert_oid = OID_OBJ(SignCert);
ObjId EncCert_oid = OID_OBJ(EncCert);
ObjId AuthCert_oid = OID_OBJ(AuthCert);
ObjId PKRoot_oid = OID_OBJ(PKRoot);
ObjId FCPath_oid = OID_OBJ(FCPath);
ObjId SerialNumber_oid = OID_OBJ(SerialNumber);
ObjId Name_oid = OID_OBJ(Name);
ObjId Uid_oid = OID_OBJ(Uid);

ObjId *Uid_OID = &Uid_oid;
ObjId *Cert_OID = &Cert_oid;

AlgId rsa_aid = {&rsa_oid, &rsa_default_keysize, nullptr};
AlgId rsaEncryption_aid = {&rsaEncryption_oid, nullptr, nullptr};
AlgId dsa_aid = {&dsa_oid, nullptr, nullptr};
AlgId md2_aid = {&md2_oid, nullptr, nullptr};
AlgId md4_aid = {&md4_oid, nullptr, nullptr};
AlgId md5_aid = {&md5_oid, nullptr, nullptr};
AlgId sha_aid = {&sha_oid, nullptr, nullptr};
AlgId sha1_aid = {&sha1_oid, nullptr, nullptr};
AlgId ripemd160_aid = {&ripemd160_oid, nullptr, nullptr};
AlgId md2WithRsaEncryption_aid = {&md2WithRsaEncryption_oid, nullptr, nullptr};
AlgId md4WithRsaEncryption_aid = {&md4WithRsaEncryption_oid, nullptr, nullptr};
AlgId md5WithRsaEncryption_aid = {&md5WithRsaEncryption_oid, nullptr, nullptr};
AlgId sha1WithRsaEncryption_aid = {&sha1WithRsaEncryption_oid, nullptr, nullptr};
AlgId md2WithRSASignature_aid = {&md2WithRSASignature_oid_obj, nullptr, nullptr};
AlgId shaWithRSASignature_aid = {&shaWithRSASignature_oid, nullptr, nullptr};
AlgId sha1WithRSASignature_aid = {&sha1WithRSASignature_oid, nullptr, nullptr};
AlgId ripemd160WithRSASignature_aid = {&ripemd160WithRSASignature_oid, nullptr, nullptr};
AlgId dsaWithSHA_aid = {&dsaWithSHA_oid, nullptr, nullptr};
AlgId dsaWithSHA1_aid = {&dsaWithSHA1_oid, nullptr, nullptr};
AlgId desECB_aid = {&desECB_oid, nullptr, nullptr};
AlgId desCBC_aid = {&desCBC_oid, nullptr, nullptr};
AlgId desCBC_pad_aid = {&desCBC_pad_oid, nullptr, nullptr};
AlgId desEDE_aid = {&desEDE_oid, nullptr, nullptr};
AlgId desCBC3_aid = {&desCBC3_oid, nullptr, nullptr};
AlgId desCBC3_pad_aid = {&desCBC3_pad_oid, nullptr, nullptr};
AlgId desEDE3CBC_aid = {&desEDE3CBC_oid, nullptr, nullptr};
AlgId idea_aid = {&idea_oid, nullptr, nullptr};
AlgId ideaCBC_aid = {&ideaCBC_oid, nullptr, nullptr};
AlgId rc2CBC_aid = {&rc2CBC_oid, nullptr, nullptr};
AlgId rc4_aid = {&rc4_oid, nullptr, nullptr};
AlgId aes128ECB_aid = {&aes128ECB_oid, nullptr, nullptr};
AlgId aes128CBC_aid = {&aes128CBC_oid, nullptr, nullptr};
AlgId aes192ECB_aid = {&aes192ECB_oid, nullptr, nullptr};
AlgId aes192CBC_aid = {&aes192CBC_oid, nullptr, nullptr};
AlgId aes256ECB_aid = {&aes256ECB_oid, nullptr, nullptr};
AlgId aes256CBC_aid = {&aes256CBC_oid, nullptr, nullptr};

} // extern "C"

namespace compat {

namespace {

using H = Hash;
using C = Cipher;

const std::vector<AlgInfo> &alg_table()
{
    static const std::vector<AlgInfo> table = {
        {"md2", oids::md2, PARM_NULL, HASH, NoAlgEnc, H::MD2, C::None, NoAlgMode, Pad::None, &md2_aid},
        {"md4", oids::md4, PARM_NULL, HASH, NoAlgEnc, H::MD4, C::None, NoAlgMode, Pad::None, &md4_aid},
        {"md5", oids::md5, PARM_NULL, HASH, NoAlgEnc, H::MD5, C::None, NoAlgMode, Pad::None, &md5_aid},
        {"sha", oids::sha, PARM_NULL, HASH, NoAlgEnc, H::SHA0, C::None, NoAlgMode, Pad::None, &sha_aid},
        {"sha1", oids::sha1, PARM_NULL, HASH, NoAlgEnc, H::SHA1, C::None, NoAlgMode, Pad::None, &sha1_aid},
        {"ripemd160", oids::ripemd160, PARM_NULL, HASH, NoAlgEnc, H::RIPEMD160, C::None, NoAlgMode, Pad::None, &ripemd160_aid},
        {"sha256", oids::sha256, PARM_NULL, HASH, NoAlgEnc, H::SHA256, C::None, NoAlgMode, Pad::None, nullptr},
        {"sha384", oids::sha384, PARM_NULL, HASH, NoAlgEnc, H::SHA384, C::None, NoAlgMode, Pad::None, nullptr},
        {"sha512", oids::sha512, PARM_NULL, HASH, NoAlgEnc, H::SHA512, C::None, NoAlgMode, Pad::None, nullptr},

        {"RSA", oids::rsa, PARM_INTEGER, ASYM_ENC, SECUDE_ALG_RSA, H::None, C::None, NoAlgMode, Pad::None, &rsa_aid},
        {"rsaEncryption", oids::rsaEncryption, PARM_NULL, ASYM_ENC, SECUDE_ALG_RSA, H::None, C::None, NoAlgMode, Pad::None, &rsaEncryption_aid},
        {"DSA", oids::dsa, PARM_KeyBits, SIG, SECUDE_ALG_DSA, H::None, C::None, NoAlgMode, Pad::None, &dsa_aid},
        {"id-dsa", oids::id_dsa, PARM_KeyBits, SIG, SECUDE_ALG_DSA, H::None, C::None, NoAlgMode, Pad::None, nullptr},

        {"md2WithRsaEncryption", {1, 2, 840, 113549, 1, 1, 2}, PARM_NULL, SIG, SECUDE_ALG_RSA, H::MD2, C::None, NoAlgMode, Pad::None, &md2WithRsaEncryption_aid},
        {"md4WithRsaEncryption", {1, 2, 840, 113549, 1, 1, 3}, PARM_NULL, SIG, SECUDE_ALG_RSA, H::MD4, C::None, NoAlgMode, Pad::None, &md4WithRsaEncryption_aid},
        {"md5WithRsaEncryption", {1, 2, 840, 113549, 1, 1, 4}, PARM_NULL, SIG, SECUDE_ALG_RSA, H::MD5, C::None, NoAlgMode, Pad::None, &md5WithRsaEncryption_aid},
        {"sha1WithRsaEncryption", {1, 2, 840, 113549, 1, 1, 5}, PARM_NULL, SIG, SECUDE_ALG_RSA, H::SHA1, C::None, NoAlgMode, Pad::None, &sha1WithRsaEncryption_aid},
        {"sha256WithRsaEncryption", {1, 2, 840, 113549, 1, 1, 11}, PARM_NULL, SIG, SECUDE_ALG_RSA, H::SHA256, C::None, NoAlgMode, Pad::None, nullptr},
        {"md2WithRSASignature", {1, 3, 14, 3, 2, 24}, PARM_NULL, SIG, SECUDE_ALG_RSA, H::MD2, C::None, NoAlgMode, Pad::None, &md2WithRSASignature_aid},
        {"md5WithRSASignature", {1, 3, 14, 3, 2, 25}, PARM_NULL, SIG, SECUDE_ALG_RSA, H::MD5, C::None, NoAlgMode, Pad::None, nullptr},
        {"shaWithRSASignature", {1, 3, 14, 3, 2, 15}, PARM_NULL, SIG, SECUDE_ALG_RSA, H::SHA0, C::None, NoAlgMode, Pad::None, &shaWithRSASignature_aid},
        {"sha1WithRSASignature", {1, 3, 14, 3, 2, 29}, PARM_NULL, SIG, SECUDE_ALG_RSA, H::SHA1, C::None, NoAlgMode, Pad::None, &sha1WithRSASignature_aid},
        {"ripemd160WithRSASignature", {1, 3, 36, 3, 3, 1, 2}, PARM_NULL, SIG, SECUDE_ALG_RSA, H::RIPEMD160, C::None, NoAlgMode, Pad::None, &ripemd160WithRSASignature_aid},
        {"dsaWithSHA", {1, 3, 14, 3, 2, 13}, PARM_ABSENT, SIG, SECUDE_ALG_DSA, H::SHA0, C::None, NoAlgMode, Pad::None, &dsaWithSHA_aid},
        {"dsaWithSHA1", {1, 3, 14, 3, 2, 27}, PARM_ABSENT, SIG, SECUDE_ALG_DSA, H::SHA1, C::None, NoAlgMode, Pad::None, &dsaWithSHA1_aid},
        {"id-dsa-with-sha1", {1, 2, 840, 10040, 4, 3}, PARM_ABSENT, SIG, SECUDE_ALG_DSA, H::SHA1, C::None, NoAlgMode, Pad::None, nullptr},

        {"desECB", {1, 3, 14, 3, 2, 6}, PARM_ABSENT, SYM_ENC, DES, H::None, C::DES, ECB, Pad::Std, &desECB_aid},
        {"desCBC", {1, 3, 14, 3, 2, 7}, PARM_OctetString, SYM_ENC, DES, H::None, C::DES, CBC, Pad::Std, &desCBC_aid},
        {"desCBC_pad", {1, 3, 36, 3, 1, 1, 2, 1}, PARM_OctetString, SYM_ENC, DES, H::None, C::DES, CBC, Pad::Pkcs, &desCBC_pad_aid},
        {"desEDE", {1, 3, 14, 3, 2, 17}, PARM_ABSENT, SYM_ENC, DES3, H::None, C::DES3, ECB, Pad::Std, &desEDE_aid},
        {"desCBC3", {1, 3, 36, 3, 1, 3, 2}, PARM_OctetString, SYM_ENC, DES3, H::None, C::DES3, CBC, Pad::Std, &desCBC3_aid},
        {"desCBC3_pad", {1, 3, 36, 3, 1, 3, 2, 1}, PARM_OctetString, SYM_ENC, DES3, H::None, C::DES3, CBC, Pad::Pkcs, &desCBC3_pad_aid},
        {"desEDE3CBC", oids::desEDE3CBC, PARM_OctetString, SYM_ENC, DES3, H::None, C::DES3, CBC, Pad::Pkcs, &desEDE3CBC_aid},
        {"idea", {1, 3, 36, 3, 1, 2}, PARM_ABSENT, SYM_ENC, IDEA, H::None, C::IDEA, ECB, Pad::Std, &idea_aid},
        {"ideaCBC", {1, 3, 6, 1, 4, 1, 188, 7, 1, 1, 2}, PARM_OctetString, SYM_ENC, IDEA, H::None, C::IDEA, CBC, Pad::Pkcs, &ideaCBC_aid},
        {"rc2CBC", {1, 2, 840, 113549, 3, 2}, PARM_RC2CBC, SYM_ENC, RC2, H::None, C::RC2, CBC, Pad::Pkcs, &rc2CBC_aid},
        {"rc4", {1, 2, 840, 113549, 3, 4}, PARM_ABSENT, SYM_ENC, RC4, H::None, C::RC4, NoAlgMode, Pad::None, &rc4_aid},
        {"aes128ECB", {2, 16, 840, 1, 101, 3, 4, 1, 1}, PARM_ABSENT, SYM_ENC, AES128, H::None, C::AES, ECB, Pad::Pkcs, &aes128ECB_aid},
        {"aes128CBC", oids::aes128CBC, PARM_OctetString, SYM_ENC, AES128, H::None, C::AES, CBC, Pad::Pkcs, &aes128CBC_aid},
        {"aes192ECB", {2, 16, 840, 1, 101, 3, 4, 1, 21}, PARM_ABSENT, SYM_ENC, AES192, H::None, C::AES, ECB, Pad::Pkcs, &aes192ECB_aid},
        {"aes192CBC", oids::aes192CBC, PARM_OctetString, SYM_ENC, AES192, H::None, C::AES, CBC, Pad::Pkcs, &aes192CBC_aid},
        {"aes256ECB", {2, 16, 840, 1, 101, 3, 4, 1, 41}, PARM_ABSENT, SYM_ENC, AES256, H::None, C::AES, ECB, Pad::Pkcs, &aes256ECB_aid},
        {"aes256CBC", oids::aes256CBC, PARM_OctetString, SYM_ENC, AES256, H::None, C::AES, CBC, Pad::Pkcs, &aes256CBC_aid},

        {"pbeWithSHA1And128BitRC4", oids::pbeSHA1RC4_128, PARM_PKCS5, PBE, RC4, H::SHA1, C::RC4, NoAlgMode, Pad::None, nullptr},
        {"pbeWithSHA1And40BitRC4", oids::pbeSHA1RC4_40, PARM_PKCS5, PBE, RC4, H::SHA1, C::RC4, NoAlgMode, Pad::None, nullptr},
        {"pbeWithSHA1AndDES3xCBC", oids::pbeSHA1DES3, PARM_PKCS5, PBE, DES3, H::SHA1, C::DES3, CBC, Pad::Pkcs, nullptr},
        {"pbeWithSHA1AndDES3CBC", oids::pbeSHA1DES2, PARM_PKCS5, PBE, DES3, H::SHA1, C::DES3, CBC, Pad::Pkcs, nullptr},
        {"pbeWithSHA1And128BitRC2CBC", oids::pbeSHA1RC2_128, PARM_PKCS5, PBE, RC2, H::SHA1, C::RC2, CBC, Pad::Pkcs, nullptr},
        {"pbeWithSHA1And40BitRC2CBC", oids::pbeSHA1RC2_40, PARM_PKCS5, PBE, RC2, H::SHA1, C::RC2, CBC, Pad::Pkcs, nullptr},
    };
    return table;
}

struct PseObjectName {
    const char *name;
    ObjId *oid;
};

const PseObjectName kPseObjects[] = {
    {SKnew_name, &SKnew_oid},
    {SKold_name, &SKold_oid},
    {SignSK_name, &SignSK_oid},
    {DecSKnew_name, &DecSKnew_oid},
    {DecSKold_name, &DecSKold_oid},
    {AuthSK_name, &AuthSK_oid},
    {Cert_name, &Cert_oid},
    {SignCert_name, &SignCert_oid},
    {EncCert_name, &EncCert_oid},
    {AuthCert_name, &AuthCert_oid},
    {PKRoot_name, &PKRoot_oid},
    {FCPath_name, &FCPath_oid},
    {SerialNumber_name, &SerialNumber_oid},
    {"Name", &Name_oid},
};

} // namespace

Oid oid_of(const ObjId *o)
{
    if (!o || o->oid_nelem <= 0 || !o->oid_elements)
        return {};
    return Oid(o->oid_elements, o->oid_elements + o->oid_nelem);
}

void fill_objid(ObjId *dst, const Oid &o)
{
    unsigned int *e = static_cast<unsigned int *>(mem_alloc(sizeof(unsigned int) * (o.size() ? o.size() : 1)));
    for (size_t i = 0; i < o.size(); ++i)
        e[i] = o[i];
    dst->oid_nelem = int(o.size());
    dst->oid_elements = e;
}

ObjId *new_objid(const Oid &o)
{
    ObjId *r = mem_new<ObjId>();
    fill_objid(r, o);
    return r;
}

std::string oid_to_string(const Oid &o)
{
    std::string s;
    for (size_t i = 0; i < o.size(); ++i) {
        if (i)
            s += '.';
        s += std::to_string(o[i]);
    }
    return s;
}

bool oid_equal(const ObjId *a, const Oid &b)
{
    if (!a)
        return false;
    if (size_t(a->oid_nelem) != b.size())
        return false;
    for (size_t i = 0; i < b.size(); ++i)
        if (a->oid_elements[i] != b[i])
            return false;
    return true;
}

bool oid_equal(const ObjId *a, const ObjId *b)
{
    return a && b && oid_equal(a, oid_of(b));
}

const AlgInfo *alg_by_oid(const Oid &o)
{
    for (const auto &a : alg_table())
        if (a.oid == o)
            return &a;
    return nullptr;
}

const AlgInfo *alg_by_oid(const ObjId *o)
{
    return o ? alg_by_oid(oid_of(o)) : nullptr;
}

const AlgInfo *alg_by_name(const char *name)
{
    if (!name)
        return nullptr;
    for (const auto &a : alg_table())
        if (!strcasecmp(a.name, name))
            return &a;
    return nullptr;
}

std::string alg_display_name(const ObjId *o)
{
    const AlgInfo *a = alg_by_oid(o);
    return a ? a->name : "unknown";
}

ObjId *pse_object_oid(const char *name)
{
    if (!name)
        return nullptr;
    for (const auto &p : kPseObjects)
        if (!std::strcmp(p.name, name))
            return p.oid;
    return nullptr;
}

const char *pse_object_name(const ObjId *oid)
{
    for (const auto &p : kPseObjects)
        if (oid_equal(oid, p.oid))
            return p.name;
    return nullptr;
}

ObjId *copy_objid(const ObjId *o)
{
    return o ? new_objid(oid_of(o)) : nullptr;
}

void free_objid_content(ObjId *o)
{
    if (!o)
        return;
    mem_free(o->oid_elements);
    o->oid_elements = nullptr;
    o->oid_nelem = 0;
}

} // namespace compat

extern "C" {

int aux_cmp_ObjId(ObjId *oid1, ObjId *oid2)
{
    if (!oid1 || !oid2)
        return oid1 == oid2 ? 0 : 1;
    return oid_equal(oid1, oid2) ? 0 : 1;
}

ObjId *aux_cpy_ObjId(ObjId *oid)
{
    return guarded<ObjId *>("aux_cpy_ObjId", nullptr, [&] { return copy_objid(oid); });
}

int aux_cpy2_ObjId(ObjId *dup_oid, ObjId *oid)
{
    return guarded<int>("aux_cpy2_ObjId", -1, [&] {
        if (!dup_oid || !oid)
            fail(EINVALID, "missing object identifier");
        fill_objid(dup_oid, oid_of(oid));
        return 0;
    });
}

void aux_free_ObjId(ObjId **oid)
{
    if (!oid || !*oid)
        return;
    if (!mem_owned(*oid)) {
        *oid = nullptr;
        return;
    }
    free_objid_content(*oid);
    mem_free(*oid);
    *oid = nullptr;
}

AlgEnc aux_ObjId2AlgEnc(ObjId *given_objid)
{
    const AlgInfo *a = alg_by_oid(given_objid);
    return a ? a->enc : NoAlgEnc;
}

AlgId *aux_Name2AlgId(char *name)
{
    const AlgInfo *a = alg_by_name(name);
    if (!a || !a->global) {
        push_error(EUNKNOWNALGID, "aux_Name2AlgId", std::string("unknown algorithm name ") + (name ? name : "(null)"));
        return nullptr;
    }
    return a->global;
}

ObjId *af_get_objoid(char *objname)
{
    ObjId *o = pse_object_oid(objname);
    if (!o)
        push_error(EOBJNAME, "af_get_objoid", std::string("unknown PSE object ") + (objname ? objname : "(null)"));
    return o;
}

} // extern "C"
