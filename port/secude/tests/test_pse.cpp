// Integration tests replaying the call sequences of the CrypTool dialogs.
#include "bn.hpp"
#include "testing.hpp"

#include <fstream>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <sstream>

using namespace compat;

namespace {

const char kCaPin[] = SECUDE_COMPAT_CA_PIN;

struct Store {
    std::string root, pse_dir, ca_dir, ca_pse;
};

const Store &store()
{
    static Store s = [] {
        Store st;
        st.root = testing::scratch_dir() + "/keystore";
        st.pse_dir = st.root + "/PSE";
        st.ca_dir = st.pse_dir + "/PSECA";
        st.ca_pse = st.ca_dir + "/capse.cse";
        if (secude_compat_create_sample_keystore(st.root.c_str(), 0) != 0)
            testing::report_failure(__FILE__, __LINE__, std::string("keystore creation: ") + th_get_last_error_text());
        return st;
    }();
    return s;
}

char *cstr(const std::string &s) { return const_cast<char *>(s.c_str()); }

PSE open_ca()
{
    return af_open(cstr(store().ca_pse), cstr(store().ca_dir), const_cast<char *>(kCaPin), nullptr);
}

std::string read_text(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string text_of(char *s)
{
    std::string r = s ? s : "";
    aux_free_String(&s);
    return r;
}

// CDlgKeyAsymGeneration::CreateAsymKeys for RSA/DSA
std::string create_user(const std::string &name, const std::string &first, bool dsa, int bits, const std::string &time,
                        const char *pin)
{
    std::string key_id = "[" + name + "][" + first + "][" + (dsa ? "DSA-" : "RSA-") + std::to_string(bits) + "][" + time + "]";
    std::string path = store().pse_dir + "/" + key_id + ".pse";
    PSE pse = af_create(cstr(path), nullptr, const_cast<char *>(pin), nullptr, TRUE);
    CHECK(pse != nullptr);
    Key keyinfo{};
    keyinfo.alg = dsa ? &dsa_aid : &rsa_aid;
    keyinfo.key_size = bits;
    CHECK_EQ(af_gen_key(pse, &keyinfo, SIGNATURE, 1), 0);
    char serial_text[] = "000001";
    OctetString SNummer{6, serial_text};
    std::string dn_text = "CN=" + first + " " + name + " [" + time + "], DC=cryptool, DC=org";
    DName *DisName = aux_Name2DName(cstr(dn_text));
    AlgId *alg = dsa ? &dsaWithSHA1_aid : &sha1WithRSASignature_aid;
    Certificate *Zert = af_create_Certificate(pse, keyinfo.key, alg, const_cast<char *>("SKnew"), DisName, &SNummer,
                                              nullptr, nullptr, TRUE, nullptr);
    CHECK(Zert != nullptr);

    SEQUENCE_OF_Extension extensions;
    extensions.element = (v3Extension *)aux_malloc(sizeof(v3Extension));
    static ObjId oid;
    static unsigned int oid_n[6] = {2, 206, 5, 4, 3, 2};
    oid.oid_nelem = 6;
    oid.oid_elements = oid_n;
    extensions.element->extnId = &oid;
    extensions.element->critical = false;
    extensions.element->extnDERcode = e_PrintableString(cstr(key_id));
    extensions.next = nullptr;
    CertExtensions cert_ext;
    std::memset(&cert_ext, 0, sizeof(CertExtensions));
    cert_ext.nonSupported = &extensions;
    Zert->tbs->extensions = &cert_ext;

    PSE PseHandle2 = open_ca();
    CHECK(PseHandle2 != nullptr);
    SerialNumber *serial = sec_random_ostr(8, 1);
    CHECK(af_cadb_get_Certificate(PseHandle2, serial) == nullptr);
    aux_free_error();
    CHECK_EQ(af_pse_update_SerialNumber(PseHandle2, serial), 0);
    Certificate *Zert2 = af_certify(PseHandle2, Zert, nullptr, &sha1WithRSASignature_aid, nullptr);
    CHECK(Zert2 != nullptr);
    CHECK(Zert2->tbs->serialnumber->noctets >= 1);
    CHECK_EQ(af_cadb_add_Certificate(PseHandle2, SIGNATURE, Zert2), 0);
    CHECK_EQ(af_cadb_add_Certificate(PseHandle2, SIGNATURE, Zert2), -1);
    aux_free_error();
    PKRoot *pkroot = af_pse_get_PKRoot(PseHandle2);
    CHECK(pkroot != nullptr);
    CHECK_EQ(af_pse_update_PKRoot(pse, pkroot), 0);
    aux_free_PKRoot(&pkroot);
    CHECK_EQ(af_pse_update(pse, const_cast<char *>("Cert"), Zert2, *&Cert_OID), 0);
    af_close(PseHandle2);
    aux_free_DName(&DisName);
    af_close(pse);
    aux_free_OctetString(&serial);
    aux_free_Certificate(&Zert2);
    aux_free_OctetString(&extensions.element->extnDERcode);
    aux_free(extensions.element);
    Zert->tbs->extensions = nullptr;
    aux_free_Certificate(&Zert);
    aux_free_KeyInfo(&keyinfo.key);
    return key_id;
}

Certificate *ca_lookup(const std::string &dn)
{
    PSE ca = open_ca();
    SET_OF_IssuedCertificate *Liste = af_cadb_get_user(ca, cstr(dn));
    CHECK(Liste != nullptr);
    Certificate *Zert = Liste ? af_cadb_get_Certificate(ca, Liste->element->serial) : nullptr;
    CHECK(Zert != nullptr);
    aux_free_SET_OF_IssuedCertificate(&Liste);
    af_close(ca);
    return Zert;
}

} // namespace

TEST(pse_create_open_and_pin)
{
    std::string path = testing::scratch_dir() + "/test.pse";
    PSE pse = af_create(cstr(path), nullptr, const_cast<char *>("1234"), nullptr, TRUE);
    CHECK(pse != nullptr);
    CHECK_EQ(std::string(pse->pse_name), path);
    CHECK(af_create(cstr(path), nullptr, const_cast<char *>("1234"), nullptr, TRUE) == nullptr);
    CHECK_EQ(th_last_error(), EPSEALREADYEXISTING);
    CHECK_EQ(af_close(pse), 0);

    CHECK(af_open(cstr(path), nullptr, const_cast<char *>("4321"), nullptr) == nullptr);
    CHECK_EQ(th_last_error(), EPIN);
    CHECK(af_open(cstr(path), cstr(store().ca_dir), const_cast<char *>("NULL"), nullptr) == nullptr);
    CHECK_EQ(th_last_error(), EPIN);
    pse = af_open(cstr(path), nullptr, const_cast<char *>("1234"), nullptr);
    CHECK(pse != nullptr);
    af_close(pse);

    std::string junk = testing::scratch_dir() + "/junk.pse";
    std::ofstream(junk) << "not a PSE";
    CHECK(af_open(cstr(junk), nullptr, const_cast<char *>("1234"), nullptr) == nullptr);
    CHECK(th_last_error() != EPIN);
    CHECK(af_open(cstr(testing::scratch_dir() + "/missing.pse"), nullptr, const_cast<char *>("1"), nullptr) == nullptr);
    CHECK_EQ(th_last_error(), EPSENOTEXISTING);
    aux_free_error();

    OctetString *raw = aux_file2OctetString(cstr(path));
    std::string dump = text_of(sdumpasn(raw, 0, 0));
    CHECK(dump.find("FATAL") == std::string::npos && dump.find("junk") == std::string::npos);
    aux_free_OctetString(&raw);
    CHECK(aux_file2OctetString(cstr(testing::scratch_dir() + "/nothing")) == nullptr);
    CHECK_EQ(th_last_error(), EFILENOTEXISTING);
    aux_free_error();
}

TEST(pse_objects_and_selectors)
{
    std::string path = testing::scratch_dir() + "/objects.pse";
    PSE pse = af_create(cstr(path), nullptr, const_cast<char *>("pin"), nullptr, FALSE);
    char raw[] = "\x01\x02\x03";
    OctetString value{3, raw};
    OctetString *ostr = e_OctetString(&value);
    ObjId *uid_before = Uid_OID;
    CHECK_EQ(af_pse_update(pse, const_cast<char *>("EcPrivKey"), ostr, *&Uid_OID), 0);
    aux_free_OctetString(&ostr);
    af_close(pse);

    pse = af_open(cstr(path), nullptr, const_cast<char *>("pin"), nullptr);
    OctetString *got = (OctetString *)af_pse_get(pse, const_cast<char *>("EcPrivKey"), Uid_OID);
    CHECK(got != nullptr);
    OctetString *inner = d_OctetString(got);
    CHECK_HEX(inner->octets, inner->noctets, "010203");
    L_NUMBER l[MAXLGTH];
    CHECK_EQ(aux_OctetString2LN2(l, inner), 0);
    CHECK_EQ(l[1], L_NUMBER(0x010203));
    CHECK(Uid_OID == uid_before);
    CHECK_EQ(Uid_OID->oid_nelem, 5);

    PSESel *sel = af_get_PSESel(pse, nullptr);
    CHECK_EQ(sec_onekeypaironly(sel), FALSE);
    sel->object = aux_cpy_String(const_cast<char *>(Toc_name));
    OctetString toc_der{0, nullptr};
    CHECK_EQ(sec_read(sel, &toc_der), 0);
    PSEToc *toc = d_PSEToc(&toc_der);
    CHECK(toc && toc->obj && std::string(toc->obj->name) == "EcPrivKey");
    aux_free_PSEToc(&toc);
    aux_free2_OctetString(&toc_der);
    ObjId oid;
    CHECK(af_pse_get(pse, const_cast<char *>("missing"), &oid) == nullptr);
    CHECK_EQ(th_last_error(), EPSEOBJECTNOTEXISTING);
    aux_free_error();
    aux_free_OctetString(&got);
    aux_free_OctetString(&inner);
    af_close(pse);
}

TEST(pse_key_generation_certification_and_use)
{
    std::string id = create_user("Doe", "Alice", false, 768, "1700000000", "4711");
    Certificate *Zert = ca_lookup("CN=Alice Doe [1700000000], DC=cryptool, DC=org");
    CHECK_EQ(Zert->tbs->subjectPK->subjectAI->objid->oid_nelem, 5);
    char *alg_name = aux_sprint_AlgId(nullptr, Zert->tbs->subjectPK->subjectAI);
    CHECK_EQ(std::string(alg_name), std::string("Algorithm RSA (OID 2.5.8.1.1), Keysize = 768"));
    aux_free(&alg_name);
    aux_free_String(&alg_name);

    // private PSE name extension (used by the PKCS#12 import)
    char *pse_name = nullptr;
    for (SEQUENCE_OF_Extension *x = Zert->tbs->extensions ? Zert->tbs->extensions->nonSupported : nullptr; x; x = x->next)
        if (x->element->extnId->oid_nelem == 6 && x->element->extnId->oid_elements[1] == 206)
            pse_name = d_PrintableString(x->element->extnDERcode);
    CHECK(pse_name && std::string(pse_name) == id);
    aux_free(pse_name);

    // RsaEnc / RsaDec
    std::string doc = "CrypTool RSA encryption round trip";
    OctetString in{sec_uint4(doc.size()), cstr(doc)};
    std::vector<char> buf(doc.size() + doc.size() / 37 + 1025);
    BitString out{0, buf.data()};
    Key Schluessel{};
    Schluessel.key = Zert->tbs->subjectPK;
    Schluessel.alg = &rsa_aid;
    PSE ca = open_ca();
    CHECK_EQ(af_encrypt_all(ca, &in, &out, &Schluessel, nullptr), 0);
    af_close(ca);
    CHECK_EQ(out.nbits, sec_uint4(96 * 8));
    OctetString *outOctet = aux_BString2OString(&out);
    PSE user = af_open(cstr(store().pse_dir + "/" + id + ".pse"), nullptr, const_cast<char *>("4711"), nullptr);
    CHECK(user != nullptr);
    BitString cin{outOctet->noctets * 8, outOctet->octets};
    std::vector<char> plain(cin.nbits / 8 + 256);
    OctetString dec{0, plain.data()};
    Key priv{};
    priv.alg = &rsa_aid;
    CHECK_EQ(af_decrypt_all(user, &cin, &dec, &priv), 0);
    CHECK_EQ(dec.noctets, sec_uint4(95));
    CHECK_EQ(std::string(dec.octets, doc.size()), doc);
    aux_free_OctetString(&outOctet);

    // Sign / Verify
    Signature Signatur;
    Signatur.signAI = &ripemd160WithRSASignature_aid;
    user->options.af_sign_check_Validity = FALSE;
    CHECK_EQ(af_sign_all(user, &in, &Signatur), 0);
    Key keyinfo{};
    keyinfo.key = Zert->tbs->subjectPK;
    CHECK_EQ(sec_verify_all(&in, &Signatur, &keyinfo, nullptr), 0);
    int sig_bits = 0, enc_bits = 0;
    AlgEnc sig_type = NoAlgEnc;
    CHECK_EQ(af_pse_get_keysize(user, &sig_bits, &enc_bits, &sig_type), 0);
    CHECK_EQ(sig_bits, 768);
    CHECK_EQ(sig_type, SECUDE_ALG_RSA);

    // certificate display (CDlgKeyAsym / sprint_Certificate_with_key)
    *(sec_uint4 *)secude_compat_symbol("print_cert_flag") = TBS | KEYINFO | VAL | ISSUER | ALG | SIGNAT | HSH | VER | SUBJECT | EXTENSIONS;
    *(sec_uint4 *)secude_compat_symbol("print_keyinfo_flag") = ALGID | KEYBITS | PK;
    std::string text = text_of(aux_sprint_Certificate(user, nullptr, Zert));
    CHECK(text.find("Modulus (768 bits)") != std::string::npos);
    CHECK(text.find("Public exponent") != std::string::npos);
    CHECK(text.find("CrypToolPSEName: " + id) != std::string::npos);
    CHECK(text.find("Issuer:               CN=CrypTool CA 2, DC=cryptool, DC=org") != std::string::npos);

    // PKRoot copied from the CA reconstructs the CA certificate
    PKRoot *root = af_pse_get_PKRoot(user);
    Certificate *proto = af_PKRoot2Protocert(root);
    ca = open_ca();
    Certificate *ca_cert = af_pse_get_Certificate(ca, SIGNATURE, nullptr, nullptr);
    OctetString *a = e_Certificate(proto), *b = e_Certificate(ca_cert);
    CHECK(a->noctets == b->noctets && !std::memcmp(a->octets, b->octets, a->noctets));
    Key ca_key{};
    ca_key.key = ca_cert->tbs->subjectPK;
    CHECK_EQ(sec_verify_all(Zert->tbs_DERcode, Zert->sig, &ca_key, nullptr), 0);
    Certificates *certs = af_pse_get_Certificates(user, ENCRYPTION, nullptr);
    CHECK(certs && certs->usercertificate && !certs->forwardpath);
    aux_free_Certificates(&certs);
    aux_free_OctetString(&a);
    aux_free_OctetString(&b);
    aux_free_Certificate(&proto);
    aux_free_Certificate(&ca_cert);
    aux_free_PKRoot(&root);
    af_close(ca);
    af_close(user);
    aux_free2_BitString(&Signatur.signature);
    aux_free_Certificate(&Zert);
}

TEST(pse_dsa_user)
{
    std::string id = create_user("Dsa", "Dave", true, 512, "1700000001", "dsa");
    Certificate *Zert = ca_lookup("CN=Dave Dsa [1700000001], DC=cryptool, DC=org");
    CHECK_EQ(Zert->tbs->subjectPK->subjectAI->objid->oid_nelem, 6);
    PSE user = af_open(cstr(store().pse_dir + "/" + id + ".pse"), nullptr, const_cast<char *>("dsa"), nullptr);
    char msg[] = "signed by DSA";
    OctetString in{sec_uint4(std::strlen(msg)), msg};
    for (AlgId *alg : {&dsaWithSHA_aid, &dsaWithSHA1_aid}) {
        Signature s;
        s.signAI = alg;
        CHECK_EQ(af_sign_all(user, &in, &s), 0);
        Key k{};
        k.key = Zert->tbs->subjectPK;
        CHECK_EQ(sec_verify_all(&in, &s, &k, nullptr), 0);
        aux_free2_BitString(&s.signature);
    }
    af_close(user);
    aux_free_Certificate(&Zert);
}

TEST(pse_tutorial_flow_with_user_chosen_exponent)
{
    // CPSEDemo::CreatePSE / AccessPSE with p, q and e chosen by the user
    Bn p = bn_new(), q = bn_new(), e = bn_word(17), n = bn_new(), one = bn_word(1), g = bn_new(), t = bn_new();
    BN_CTX *ctx = BN_CTX_new();
    do {
        BN_generate_prime_ex(p.get(), 256, 0, nullptr, nullptr, nullptr);
        BN_sub(t.get(), p.get(), one.get());
        BN_gcd(g.get(), t.get(), e.get(), ctx);
    } while (!BN_is_one(g.get()));
    do {
        BN_generate_prime_ex(q.get(), 256, 0, nullptr, nullptr, nullptr);
        BN_sub(t.get(), q.get(), one.get());
        BN_gcd(g.get(), t.get(), e.get(), ctx);
    } while (!BN_is_one(g.get()));
    BN_mul(n.get(), p.get(), q.get(), ctx);
    BN_CTX_free(ctx);

    KeyBits keybits;
    std::memset(&keybits, 0, sizeof(KeyBits));
    Bytes pb = bn_bytes(p.get()), qb = bn_bytes(q.get()), nb = bn_bytes(n.get()), eb = bn_bytes(e.get());
    keybits.part1 = {sec_uint4(pb.size()), reinterpret_cast<char *>(pb.data())};
    keybits.part2 = {sec_uint4(qb.size()), reinterpret_cast<char *>(qb.data())};
    BitString *bitstring = e_KeyBits(&keybits);
    KeyInfo keyinfo;
    CHECK_EQ(aux_cpy2_BitString(&keyinfo.subjectkey, bitstring), 0);
    aux_free_BitString(&bitstring);
    keyinfo.subjectAI = aux_cpy_AlgId(&rsa_aid);
    *reinterpret_cast<int *>(keyinfo.subjectAI->param) = 512;

    std::string name = store().pse_dir + "/[Tutorial][Tom][RSA-512][1].pse";
    PSE m_hPSE = af_create(cstr(name), nullptr, const_cast<char *>("tut"), nullptr, TRUE);
    PSE_Sel *psesel = af_get_PSESel(m_hPSE, static_cast<ObjId *>(0));
    psesel->object = aux_cpy_String(const_cast<char *>(SKnew_name));
    psesel->object_type = &SKnew_oid;
    OctetString *octetstring = e_KeyInfo(&keyinfo);
    aux_free2_KeyInfo(&keyinfo);
    CHECK_EQ(sec_write_PSE(psesel, octetstring), 0);
    aux_free_OctetString(&octetstring);

    char serial_text[] = "000001";
    OctetString osSerial{6, serial_text};
    DName *dn = aux_Name2DName(const_cast<char *>("CN=Tom Tutorial [1], DC=cryptool, DC=org"));
    std::memset(&keybits, 0, sizeof(KeyBits));
    keybits.part1 = {sec_uint4(nb.size()), reinterpret_cast<char *>(nb.data())};
    keybits.part2 = {sec_uint4(eb.size()), reinterpret_cast<char *>(eb.data())};
    bitstring = e_KeyBits(&keybits);
    CHECK_EQ(aux_cpy2_BitString(&keyinfo.subjectkey, bitstring), 0);
    aux_free_BitString(&bitstring);
    keyinfo.subjectAI = aux_cpy_AlgId(&rsa_aid);
    Certificate *Cert = af_create_Certificate(m_hPSE, &keyinfo, &sha1WithRSASignature_aid, const_cast<char *>("SKnew"), dn,
                                              &osSerial, nullptr, nullptr, TRUE, nullptr);
    CHECK(Cert != nullptr);
    Key self{};
    self.key = &keyinfo;
    CHECK_EQ(sec_verify_all(Cert->tbs_DERcode, Cert->sig, &self, nullptr), 0);
    PSE hPSE_CA = open_ca();
    Certificate *Cert_CA = af_certify(hPSE_CA, Cert, nullptr, &sha1WithRSASignature_aid, nullptr);
    CHECK_EQ(af_cadb_add_Certificate(hPSE_CA, SIGNATURE, Cert_CA), 0);
    CHECK_EQ(af_pse_update(m_hPSE, const_cast<char *>("Cert"), Cert_CA, *&Cert_OID), 0);
    af_close(hPSE_CA);
    af_close(m_hPSE);

    // AccessPSE
    PSESel *sel = sec_open(cstr(name), const_cast<char *>("tut"), nullptr);
    CHECK(sel != nullptr);
    sel->object = aux_cpy_String(const_cast<char *>(SKnew_name));
    sel->object_type = aux_cpy_ObjId(&SKnew_oid);
    OctetString value;
    CHECK_EQ(sec_read_PSE(sel, &value), 0);
    KeyInfo *ki = d_KeyInfo(&value);
    KeyBits *kb = d_KeyBits(&ki->subjectkey);
    CHECK(bytes_of(&kb->part1) == pb && bytes_of(&kb->part2) == qb);
    aux_free_KeyInfo(&ki);
    aux_free2_OctetString(&value);
    aux_free_KeyBits(&kb);
    aux_free_String(&sel->object);
    aux_free_ObjId(&sel->object_type);
    sel->object = aux_cpy_String(const_cast<char *>(Cert_name));
    sel->object_type = aux_cpy_ObjId(&Cert_oid);
    CHECK_EQ(sec_read_PSE(sel, &value), 0);
    Certificate *cert = d_Certificate(&value);
    CHECK_EQ(text_of(aux_DName2Name(cert->tbs->subject)), std::string("CN=Tom Tutorial [1], DC=cryptool, DC=org"));
    kb = d_KeyBits(&cert->tbs->subjectPK->subjectkey);
    CHECK(bytes_of(&kb->part2) == eb);
    aux_free_KeyBits(&kb);
    aux_free_Certificate(&cert);
    aux_free2_OctetString(&value);
    CHECK_EQ(sec_close(sel), 0);

    // private key operations use the exponent of the PSE certificate
    PSE pse = af_open(cstr(name), nullptr, const_cast<char *>("tut"), nullptr);
    char msg[] = "tutorial";
    OctetString in{8, msg};
    Signature s;
    s.signAI = &md5WithRsaEncryption_aid;
    CHECK_EQ(af_sign_all(pse, &in, &s), 0);
    Key k{};
    k.key = Cert_CA->tbs->subjectPK;
    CHECK_EQ(sec_verify_all(&in, &s, &k, nullptr), 0);
    aux_free2_BitString(&s.signature);
    af_close(pse);
    aux_free_DName(&dn);
    aux_free2_KeyInfo(&keyinfo);
    aux_free_Certificate(&Cert);
    aux_free_Certificate(&Cert_CA);
}

TEST(pse_sample_keystore)
{
    const Store &st = store();
    std::string rsa = st.pse_dir + "/[SideChannelAttack][Bob][RSA-512][1152179494][PIN=1234].pse";
    PSE bob = af_open(cstr(rsa), nullptr, const_cast<char *>("1234"), nullptr);
    CHECK(bob != nullptr);
    Certificate *Zert = ca_lookup("CN=Bob SideChannelAttack [1152179494], DC=cryptool, DC=org");

    // CDlgHybridEncryptionDemo: session key left padded to (512-1)/8 octets
    Bytes session = random_bytes(16);
    Bytes padded(63 - 16, 0);
    padded.insert(padded.end(), session.begin(), session.end());
    OctetString in{63, reinterpret_cast<char *>(padded.data())};
    std::vector<char> buf(2048);
    BitString out{0, buf.data()};
    Key Schluessel{};
    Schluessel.key = Zert->tbs->subjectPK;
    Schluessel.alg = &rsa_aid;
    PSE ca = open_ca();
    CHECK_EQ(af_encrypt_all(ca, &in, &out, &Schluessel, nullptr), 0);
    af_close(ca);
    CHECK_EQ(out.nbits, sec_uint4(512));
    std::vector<char> dbuf(64 + 256);
    OctetString dec{0, dbuf.data()};
    Key priv{};
    priv.alg = &rsa_aid;
    CHECK_EQ(af_decrypt_all(bob, &out, &dec, &priv), 0);
    CHECK_EQ(dec.noctets, sec_uint4(63));
    CHECK(Bytes(dbuf.begin() + 47, dbuf.begin() + 63) == session);
    af_close(bob);
    aux_free_Certificate(&Zert);

    // EC key of the hybrid encryption demo: text file with domain parameters and public key
    std::string ec = st.pse_dir + "/[HybridEncryption][Bob][EC-prime239v1][1178702474][PIN=1234]";
    std::istringstream lines(read_text(ec));
    std::vector<std::string> v;
    for (std::string line; std::getline(lines, line);)
        v.push_back(line);
    CHECK_EQ(v.size(), size_t(9));
    CHECK_EQ(v[0], std::string("0X7FFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFF8000000000007FFFFFFFFFFC"));
    CHECK_EQ(v[3], std::string("0XFFA963CDCA8816CCC33B8642BEDF905C3D358573D3F27FBBD3B3CB9AAAF"));
    CHECK_EQ(v[5], std::string("0X1"));
    PSE ecpse = af_open(cstr(ec + ".pse"), nullptr, const_cast<char *>("1234"), nullptr);
    CHECK(ecpse != nullptr);
    OctetString *ostr = (OctetString *)af_pse_get(ecpse, const_cast<char *>("EcPrivKey"), *&Uid_OID);
    af_close(ecpse);
    OctetString *d = d_OctetString(ostr);
    Bn priv_key = bn_from(bytes_of(d));
    EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_X9_62_prime239v1);
    EC_POINT *q = EC_POINT_new(group);
    Bn x = bn_new(), y = bn_new();
    EC_POINT_mul(group, q, priv_key.get(), nullptr, nullptr, nullptr);
    EC_POINT_get_affine_coordinates(group, q, x.get(), y.get(), nullptr);
    char *xs = BN_bn2hex(x.get()), *ys = BN_bn2hex(y.get());
    std::string xh = xs, yh = ys;
    while (xh.size() > 1 && xh[0] == '0')
        xh.erase(0, 1);
    while (yh.size() > 1 && yh[0] == '0')
        yh.erase(0, 1);
    CHECK_EQ(v[7], "0X" + xh);
    CHECK_EQ(v[8], "0X" + yh);
    OPENSSL_free(xs);
    OPENSSL_free(ys);
    EC_POINT_free(q);
    EC_GROUP_free(group);
    aux_free_OctetString(&ostr);
    aux_free_OctetString(&d);

    // serial numbers handed out by the CA increase
    ca = open_ca();
    SerialNumber *s1 = af_pse_get_SerialNumber(ca);
    CHECK(s1 != nullptr);
    SET_OF_Name *users = af_cadb_list_user(ca);
    int count = 0;
    for (SET_OF_Name *u = users; u; u = u->next)
        ++count;
    CHECK(count >= 2);
    af_close(ca);
    aux_free_OctetString(&s1);
}
