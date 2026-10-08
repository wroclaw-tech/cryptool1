#include "der.hpp"
#include "testing.hpp"

using namespace compat;

namespace {

std::string str(char *s)
{
    std::string r = s ? s : "(null)";
    aux_free(s);
    return r;
}

} // namespace

TEST(encode_octetstring_and_printable)
{
    char data[] = "hello";
    OctetString in{5, data};
    OctetString *e = e_OctetString(&in);
    CHECK_HEX(e->octets, e->noctets, "040568656c6c6f");
    OctetString *d = d_OctetString(e);
    CHECK_HEX(d->octets, d->noctets, "68656c6c6f");
    aux_free_OctetString(&e);
    aux_free_OctetString(&d);
    CHECK(e == nullptr);

    char name[] = "[SideChannelAttack][Bob][RSA-512][1152179494][PIN=1234]";
    OctetString *p = e_PrintableString(name);
    CHECK_EQ(uint8_t(p->octets[0]), uint8_t(0x13));
    CHECK_EQ(str(d_PrintableString(p)), std::string(name));
    aux_free_OctetString(&p);
}

TEST(encode_keybits_keyinfo_algid)
{
    uint8_t n[] = {0xc0, 0x01, 0x02}, ev[] = {0x01, 0x00, 0x01};
    KeyBits kb{};
    kb.part1 = {3, reinterpret_cast<char *>(n)};
    kb.part2 = {3, reinterpret_cast<char *>(ev)};
    BitString *bs = e_KeyBits(&kb);
    CHECK_HEX(bs->bits, bs->nbits / 8, "300b020400c001020203010001");
    KeyBits *back = d_KeyBits(bs);
    CHECK_EQ(back->choice, 2);
    CHECK_HEX(back->part1.octets, back->part1.noctets, "c00102");
    CHECK_HEX(back->part2.octets, back->part2.noctets, "010001");
    aux_free_KeyBits(&back);

    KeyInfo ki{};
    ki.subjectAI = aux_cpy_AlgId(&rsa_aid);
    *static_cast<unsigned int *>(ki.subjectAI->param) = 1024;
    CHECK(aux_cpy2_BitString(&ki.subjectkey, bs) == 0);
    OctetString *enc = e_KeyInfo(&ki);
    KeyInfo *dec = d_KeyInfo(enc);
    CHECK_EQ(aux_cmp_ObjId(dec->subjectAI->objid, rsa_aid.objid), 0);
    CHECK_EQ(*static_cast<unsigned int *>(dec->subjectAI->param), 1024u);
    CHECK_EQ(str(aux_sprint_AlgId(nullptr, dec->subjectAI)), std::string("Algorithm RSA (OID 2.5.8.1.1), Keysize = 1024"));
    CHECK_EQ(str(aux_sprint_AlgId(nullptr, &sha1WithRsaEncryption_aid)),
             std::string("Algorithm sha1WithRsaEncryption (OID 1.2.840.113549.1.1.5), NULL"));
    CHECK_EQ(aux_ObjId2AlgEnc(dec->subjectAI->objid), SECUDE_ALG_RSA);
    CHECK_EQ(aux_ObjId2AlgEnc(dsa_aid.objid), SECUDE_ALG_DSA);
    CHECK_EQ(aux_ObjId2AlgEnc(idea_aid.objid), IDEA);
    CHECK_EQ(rsa_aid.objid->oid_nelem, 5);
    CHECK_EQ(dsa_aid.objid->oid_nelem, 6);
    aux_free_KeyInfo(&dec);
    aux_free_OctetString(&enc);
    aux_free2_KeyInfo(&ki);
    aux_free_BitString(&bs);

    OctetString *a = e_AlgId(&md5WithRsaEncryption_aid);
    CHECK_HEX(a->octets, a->noctets, "300d06092a864886f70d0101040500");
    aux_free_OctetString(&a);
    a = e_AlgId(&dsaWithSHA1_aid);
    CHECK_HEX(a->octets, a->noctets, "300706052b0e03021b");
    aux_free_OctetString(&a);
    CHECK(aux_Name2AlgId(const_cast<char *>("desCBC_pad")) == &desCBC_pad_aid);
}

TEST(encode_distinguished_names)
{
    const char *text = "CN=Bob SideChannelAttack [1152179494], DC=cryptool, DC=org";
    DName *dn = aux_Name2DName(const_cast<char *>(text));
    CHECK(dn != nullptr);
    Bytes der_bytes = enc_dname(dn);
    // most significant RDN first in DER: DC=org
    CHECK_EQ(testing::hex(der_bytes.data(), der_bytes.size()).find("060a0992268993f22c6401191603" "6f7267"), size_t(12));
    CHECK_EQ(str(aux_DName2Name(dn)), std::string(text));
    CHECK_EQ(str(aux_DName2Attr(dn, const_cast<char *>("CN"))), std::string("Bob SideChannelAttack [1152179494]"));
    DName *other = aux_Name2DName(const_cast<char *>("cn=bob  sidechannelattack [1152179494],DC=cryptool,DC=org"));
    CHECK_EQ(aux_cmp_DName(dn, other), 0);
    DName *diff = aux_Name2DName(const_cast<char *>("CN=Alice, DC=cryptool, DC=org"));
    CHECK(aux_cmp_DName(dn, diff) != 0);
    aux_free_DName(&dn);
    aux_free_DName(&other);
    aux_free_DName(&diff);
    CHECK(dn == nullptr);
}

TEST(encode_latin1_unicode)
{
    OctetString *u = aux_latin1_to_unicode(const_cast<char *>("A\xe4"), TRUE);
    CHECK_HEX(u->octets, u->noctets, "004100e40000");
    aux_free_OctetString(&u);
}

TEST(encode_asn1_dump)
{
    Bytes good = der::seq({der::integer(5), der::oid(oids::sha1), der::octet_string(der::seq({der::null()}))});
    OctetString o{sec_uint4(good.size()), reinterpret_cast<char *>(good.data())};
    std::string dump = str(sdumpasn(&o, 0, 0));
    CHECK(dump.find("SEQUENCE {") != std::string::npos);
    CHECK(dump.find("1.3.14.3.2.26 (sha1)") != std::string::npos);
    CHECK(dump.find("encapsulates") != std::string::npos);
    CHECK(dump.find("FATAL") == std::string::npos);

    Bytes junk = good;
    junk.push_back(0);
    o = {sec_uint4(junk.size()), reinterpret_cast<char *>(junk.data())};
    CHECK(str(sdumpasn(&o, 0, 0)).find("junk at end of PDU") != std::string::npos);
    char text[] = "plain text, not ASN.1";
    o = {sec_uint4(std::strlen(text)), text};
    CHECK(str(sdumpasn(&o, 0, 0)).find("FATAL: decoding failed!") != std::string::npos);

    char buf[] = "0123456789abcdefXYZ";
    std::string hexd = str(aux_sxdump(nullptr, buf, 19, 0));
    CHECK(hexd.find("30313233 34353637  38396162 63646566") != std::string::npos);
    CHECK(hexd.find("0123456789abcdef") != std::string::npos);
}

TEST(memory_safety_of_free_functions)
{
    // CrypTool passes addresses of stack variables and globals to free functions
    char *s = aux_cpy_String(const_cast<char *>("x"));
    aux_free(&s);
    aux_free_String(&s);
    CHECK(s == nullptr);
    ObjId *global = &SKnew_oid;
    aux_free_ObjId(&global);
    CHECK(global == nullptr);
    CHECK_EQ(SKnew_oid.oid_nelem, 5);
    AlgId *g = &rsa_aid;
    aux_free_AlgId(&g);
    CHECK(rsa_aid.param != nullptr);
}
