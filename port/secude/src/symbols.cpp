// Name -> address table of all SECUDE functions and data objects used by
// CrypTool (the DoAll list in CrypTool/SecudeLib.h) and libec.
#include "internal.hpp"

namespace {

struct Symbol {
    const char *name;
    void *address;
};

const std::vector<Symbol> &symbols()
{
    static const std::vector<Symbol> table = {
        {"SECUDE_HasValidTicket", reinterpret_cast<void *>(&SECUDE_HasValidTicket)},
        {"light_version", reinterpret_cast<void *>(&light_version)},
        {"aux_sprint_version", reinterpret_cast<void *>(&aux_sprint_version)},
        {"aux_sxdump2", reinterpret_cast<void *>(&aux_sxdump2)},
        {"aux_sxdump", reinterpret_cast<void *>(&aux_sxdump)},
        {"d_KeyBits", reinterpret_cast<void *>(&d_KeyBits)},
        {"af_cadb_add_Certificate", reinterpret_cast<void *>(&af_cadb_add_Certificate)},
        {"aux_free_OctetString", reinterpret_cast<void *>(&aux_free_OctetString)},
        {"aux_alloc_OctetString", reinterpret_cast<void *>(&aux_alloc_OctetString)},
        {"sec_verify_all", reinterpret_cast<void *>(&sec_verify_all)},
        {"arithmetic_comp", reinterpret_cast<void *>(&arithmetic_comp)},
        {"af_cadb_list_user", reinterpret_cast<void *>(&af_cadb_list_user)},
        {"af_pse_get", reinterpret_cast<void *>(&af_pse_get)},
        {"af_pse_update", reinterpret_cast<void *>(&af_pse_update)},
        {"af_pse_update_SerialNumber", reinterpret_cast<void *>(&af_pse_update_SerialNumber)},
        {"e_OctetString", reinterpret_cast<void *>(&e_OctetString)},
        {"lngtouse", reinterpret_cast<void *>(&lngtouse)},
        {"aux_OctetString2LN2", reinterpret_cast<void *>(&aux_OctetString2LN2)},
        {"d_OctetString", reinterpret_cast<void *>(&d_OctetString)},
        {"aux_free_DName", reinterpret_cast<void *>(&aux_free_DName)},
        {"aux_OctetString2file", reinterpret_cast<void *>(&aux_OctetString2file)},
        {"af_certify", reinterpret_cast<void *>(&af_certify)},
        {"af_create_Certificate", reinterpret_cast<void *>(&af_create_Certificate)},
        {"aux_Name2DName", reinterpret_cast<void *>(&aux_Name2DName)},
        {"af_gen_key", reinterpret_cast<void *>(&af_gen_key)},
        {"sec_encrypt_all", reinterpret_cast<void *>(&sec_encrypt_all)},
        {"sec_decrypt_all", reinterpret_cast<void *>(&sec_decrypt_all)},
        {"arithmetic_mexp", reinterpret_cast<void *>(&arithmetic_mexp)},
        {"arithmetic_shift", reinterpret_cast<void *>(&arithmetic_shift)},
        {"aux_OString2BString", reinterpret_cast<void *>(&aux_OString2BString)},
        {"aux_BitString2LN2", reinterpret_cast<void *>(&aux_BitString2LN2)},
        {"arithmetic_sub", reinterpret_cast<void *>(&arithmetic_sub)},
        {"rndm", reinterpret_cast<void *>(&rndm)},
        {"arithmetic_msub", reinterpret_cast<void *>(&arithmetic_msub)},
        {"arithmetic_mdiv", reinterpret_cast<void *>(&arithmetic_mdiv)},
        {"arithmetic_mmult", reinterpret_cast<void *>(&arithmetic_mmult)},
        {"arithmetic_madd", reinterpret_cast<void *>(&arithmetic_madd)},
        {"arithmetic_div", reinterpret_cast<void *>(&arithmetic_div)},
        {"arithmetic_mult", reinterpret_cast<void *>(&arithmetic_mult)},
        {"arithmetic_add", reinterpret_cast<void *>(&arithmetic_add)},
        {"sec_hash_all", reinterpret_cast<void *>(&sec_hash_all)},
        {"sec_hash_init", reinterpret_cast<void *>(&sec_hash_init)},
        {"sec_hash_more", reinterpret_cast<void *>(&sec_hash_more)},
        {"sec_hash_end", reinterpret_cast<void *>(&sec_hash_end)},
        {"aux_sprint_Certificate", reinterpret_cast<void *>(&aux_sprint_Certificate)},
        {"aux_sprint_error", reinterpret_cast<void *>(&aux_sprint_error)},
        {"aux_LN2OctetString", reinterpret_cast<void *>(&aux_LN2OctetString)},
        {"af_create", reinterpret_cast<void *>(&af_create)},
        {"sec_create", reinterpret_cast<void *>(&sec_create)},
        {"af_cadb_get_Certificate", reinterpret_cast<void *>(&af_cadb_get_Certificate)},
        {"aux_BString2OString", reinterpret_cast<void *>(&aux_BString2OString)},
        {"aux_free_Certificate", reinterpret_cast<void *>(&aux_free_Certificate)},
        {"aux_free_Certificates", reinterpret_cast<void *>(&aux_free_Certificates)},
        {"d_ContentInfo", reinterpret_cast<void *>(&d_ContentInfo)},
        {"aux_free_SET_OF_IssuedCertificate", reinterpret_cast<void *>(&aux_free_SET_OF_IssuedCertificate)},
        {"af_close", reinterpret_cast<void *>(&af_close)},
        {"sec_close", reinterpret_cast<void *>(&sec_close)},
        {"af_encrypt_all", reinterpret_cast<void *>(&af_encrypt_all)},
        {"af_cadb_get_user", reinterpret_cast<void *>(&af_cadb_get_user)},
        {"th_get_last_error_text", reinterpret_cast<void *>(&th_get_last_error_text)},
        {"th_last_error", reinterpret_cast<void *>(&th_last_error)},
        {"af_open", reinterpret_cast<void *>(&af_open)},
        {"aux_file2OctetString", reinterpret_cast<void *>(&aux_file2OctetString)},
        {"sdumpasn", reinterpret_cast<void *>(&sdumpasn)},
        {"aux_adump", reinterpret_cast<void *>(&aux_adump)},
        {"af_pse_get_Name", reinterpret_cast<void *>(&af_pse_get_Name)},
        {"aux_DName2Attr", reinterpret_cast<void *>(&aux_DName2Attr)},
        {"af_pse_get_SerialNumber", reinterpret_cast<void *>(&af_pse_get_SerialNumber)},
        {"af_pse_get_Certificates", reinterpret_cast<void *>(&af_pse_get_Certificates)},
        {"af_pse_get_CrlSet", reinterpret_cast<void *>(&af_pse_get_CrlSet)},
        {"d_PSEToc", reinterpret_cast<void *>(&d_PSEToc)},
        {"af_decrypt_all", reinterpret_cast<void *>(&af_decrypt_all)},
        {"af_sign_all", reinterpret_cast<void *>(&af_sign_all)},
        {"aux_free", reinterpret_cast<void *>(&aux_free)},
        {"th_remove_last_error", reinterpret_cast<void *>(&th_remove_last_error)},
        {"aux_free_BitString", reinterpret_cast<void *>(&aux_free_BitString)},
        {"aux_free_AlgId", reinterpret_cast<void *>(&aux_free_AlgId)},
        {"aux_free_KeyInfo", reinterpret_cast<void *>(&aux_free_KeyInfo)},
        {"aux_free_ObjId", reinterpret_cast<void *>(&aux_free_ObjId)},
        {"aux_free_KeyBits", reinterpret_cast<void *>(&aux_free_KeyBits)},
        {"aux_free_PSEToc", reinterpret_cast<void *>(&aux_free_PSEToc)},
        {"aux_cpy_ObjId", reinterpret_cast<void *>(&aux_cpy_ObjId)},
        {"aux_cpy_String", reinterpret_cast<void *>(&aux_cpy_String)},
        {"aux_free_String", reinterpret_cast<void *>(&aux_free_String)},
        {"sec_onekeypaironly", reinterpret_cast<void *>(&sec_onekeypaironly)},
        {"af_get_PSESel", reinterpret_cast<void *>(&af_get_PSESel)},
        {"aux_latin1_to_unicode", reinterpret_cast<void *>(&aux_latin1_to_unicode)},
        {"aux_calloc", reinterpret_cast<void *>(&aux_calloc)},
        {"aux_malloc", reinterpret_cast<void *>(&aux_malloc)},
        {"aux_cpy_OctetString", reinterpret_cast<void *>(&aux_cpy_OctetString)},
        {"aux_create_PrivateKeyInfo", reinterpret_cast<void *>(&aux_create_PrivateKeyInfo)},
        {"af_pse_get_FCPath", reinterpret_cast<void *>(&af_pse_get_FCPath)},
        {"aux_free_FCPath", reinterpret_cast<void *>(&aux_free_FCPath)},
        {"af_pse_get_PKRoot", reinterpret_cast<void *>(&af_pse_get_PKRoot)},
        {"af_PKRoot2Protocert", reinterpret_cast<void *>(&af_PKRoot2Protocert)},
        {"aux_free_PKRoot", reinterpret_cast<void *>(&aux_free_PKRoot)},
        {"aux_free_RSAPrivateKey", reinterpret_cast<void *>(&aux_free_RSAPrivateKey)},
        {"pkcs12_encode", reinterpret_cast<void *>(&pkcs12_encode)},
        {"aux_free_error", reinterpret_cast<void *>(&aux_free_error)},
        {"af_pse_update_PKRoot", reinterpret_cast<void *>(&af_pse_update_PKRoot)},
        {"aux_cmp_DName", reinterpret_cast<void *>(&aux_cmp_DName)},
        {"aux_create_PKRoot", reinterpret_cast<void *>(&aux_create_PKRoot)},
        {"sec_write_PSE", reinterpret_cast<void *>(&sec_write_PSE)},
        {"e_FCPath", reinterpret_cast<void *>(&e_FCPath)},
        {"e_Certificate", reinterpret_cast<void *>(&e_Certificate)},
        {"aux_ObjId2AlgEnc", reinterpret_cast<void *>(&aux_ObjId2AlgEnc)},
        {"d_RSAPrivateKey", reinterpret_cast<void *>(&d_RSAPrivateKey)},
        {"e_KeyBits", reinterpret_cast<void *>(&e_KeyBits)},
        {"aux_cpy_AlgId", reinterpret_cast<void *>(&aux_cpy_AlgId)},
        {"e_KeyInfo", reinterpret_cast<void *>(&e_KeyInfo)},
        {"e_PKRoot", reinterpret_cast<void *>(&e_PKRoot)},
        {"pkcs12_decode", reinterpret_cast<void *>(&pkcs12_decode)},
        {"af_pse_get_Certificate", reinterpret_cast<void *>(&af_pse_get_Certificate)},
        {"aux_DName2Name", reinterpret_cast<void *>(&aux_DName2Name)},
        {"aux_sprint_AlgId", reinterpret_cast<void *>(&aux_sprint_AlgId)},
        {"e_PrintableString", reinterpret_cast<void *>(&e_PrintableString)},
        {"d_PrintableString", reinterpret_cast<void *>(&d_PrintableString)},
        {"aux_cmp_ObjId", reinterpret_cast<void *>(&aux_cmp_ObjId)},
        {"af_get_objoid", reinterpret_cast<void *>(&af_get_objoid)},
        {"af_pse_get_keysize", reinterpret_cast<void *>(&af_pse_get_keysize)},
        {"sec_write", reinterpret_cast<void *>(&sec_write)},
        {"sec_read", reinterpret_cast<void *>(&sec_read)},
        {"sec_read_PSE", reinterpret_cast<void *>(&sec_read_PSE)},
        {"d_KeyInfo", reinterpret_cast<void *>(&d_KeyInfo)},
        {"aux_Name2AlgId", reinterpret_cast<void *>(&aux_Name2AlgId)},
        {"af_get_options", reinterpret_cast<void *>(&af_get_options)},
        {"aux_cpy2_AlgId", reinterpret_cast<void *>(&aux_cpy2_AlgId)},
        {"aux_free2_AlgId", reinterpret_cast<void *>(&aux_free2_AlgId)},
        {"aux_cpy2_ObjId", reinterpret_cast<void *>(&aux_cpy2_ObjId)},
        {"aux_free2_BitString", reinterpret_cast<void *>(&aux_free2_BitString)},
        {"aux_free_PSESel", reinterpret_cast<void *>(&aux_free_PSESel)},
        {"e_AlgId", reinterpret_cast<void *>(&e_AlgId)},
        {"rsa_sign_all", reinterpret_cast<void *>(&rsa_sign_all)},
        {"sec_get_key", reinterpret_cast<void *>(&sec_get_key)},
        {"sec_open", reinterpret_cast<void *>(&sec_open)},
        {"d_Certificate", reinterpret_cast<void *>(&d_Certificate)},
        {"aux_free2_KeyBits", reinterpret_cast<void *>(&aux_free2_KeyBits)},
        {"aux_free2_KeyInfo", reinterpret_cast<void *>(&aux_free2_KeyInfo)},
        {"aux_free2_OctetString", reinterpret_cast<void *>(&aux_free2_OctetString)},
        {"aux_cpy2_BitString", reinterpret_cast<void *>(&aux_cpy2_BitString)},
        {"sec_MD2Init", reinterpret_cast<void *>(&sec_MD2Init)},
        {"sec_MD2Update", reinterpret_cast<void *>(&sec_MD2Update)},
        {"sec_MD2Final", reinterpret_cast<void *>(&sec_MD2Final)},
        {"sec_MD4Init", reinterpret_cast<void *>(&sec_MD4Init)},
        {"sec_MD4Update", reinterpret_cast<void *>(&sec_MD4Update)},
        {"sec_MD4Final", reinterpret_cast<void *>(&sec_MD4Final)},
        {"sec_MD5Init", reinterpret_cast<void *>(&sec_MD5Init)},
        {"sec_MD5Update", reinterpret_cast<void *>(&sec_MD5Update)},
        {"sec_MD5Final", reinterpret_cast<void *>(&sec_MD5Final)},
        {"shsInit", reinterpret_cast<void *>(&shsInit)},
        {"shsUpdate", reinterpret_cast<void *>(&shsUpdate)},
        {"shsFinal", reinterpret_cast<void *>(&shsFinal)},
        {"shs1Init", reinterpret_cast<void *>(&shs1Init)},
        {"shs1Update", reinterpret_cast<void *>(&shs1Update)},
        {"shs1Final", reinterpret_cast<void *>(&shs1Final)},
        {"sec_random_ostr", reinterpret_cast<void *>(&sec_random_ostr)},
        // called directly by libec (s_ecvali.c)
        {"global_add_error", reinterpret_cast<void *>(&global_add_error)},
        {"rabinstest", reinterpret_cast<void *>(&rabinstest)},
        {"ripemd160WithRSASignature_aid", static_cast<void *>(&ripemd160WithRSASignature_aid)},
        {"shaWithRSASignature_aid", static_cast<void *>(&shaWithRSASignature_aid)},
        {"md2WithRsaEncryption_aid", static_cast<void *>(&md2WithRsaEncryption_aid)},
        {"md2WithRSASignature_aid", static_cast<void *>(&md2WithRSASignature_aid)},
        {"dsa_aid", static_cast<void *>(&dsa_aid)},
        {"rc2CBC_aid", static_cast<void *>(&rc2CBC_aid)},
        {"rc4_aid", static_cast<void *>(&rc4_aid)},
        {"md5WithRsaEncryption_aid", static_cast<void *>(&md5WithRsaEncryption_aid)},
        {"desEDE_aid", static_cast<void *>(&desEDE_aid)},
        {"desCBC3_aid", static_cast<void *>(&desCBC3_aid)},
        {"desCBC_pad_aid", static_cast<void *>(&desCBC_pad_aid)},
        {"desECB_aid", static_cast<void *>(&desECB_aid)},
        {"idea_aid", static_cast<void *>(&idea_aid)},
        {"md2_aid", static_cast<void *>(&md2_aid)},
        {"md4_aid", static_cast<void *>(&md4_aid)},
        {"md5_aid", static_cast<void *>(&md5_aid)},
        {"sha_aid", static_cast<void *>(&sha_aid)},
        {"sha1_aid", static_cast<void *>(&sha1_aid)},
        {"ripemd160_aid", static_cast<void *>(&ripemd160_aid)},
        {"sha1WithRSASignature_aid", static_cast<void *>(&sha1WithRSASignature_aid)},
        {"rsa_aid", static_cast<void *>(&rsa_aid)},
        {"rsaEncryption_aid", static_cast<void *>(&rsaEncryption_aid)},
        {"dsaWithSHA_aid", static_cast<void *>(&dsaWithSHA_aid)},
        {"dsaWithSHA1_aid", static_cast<void *>(&dsaWithSHA1_aid)},
        {"Uid_OID", static_cast<void *>(&Uid_OID)},
        {"Cert_OID", static_cast<void *>(&Cert_OID)},
        {"sha1_oid", static_cast<void *>(&sha1_oid)},
        {"pbeWithSHA1And40BitRC2CBC_oid", static_cast<void *>(&pbeWithSHA1And40BitRC2CBC_oid)},
        {"pbeWithSHA1AndDES3xCBC_oid", static_cast<void *>(&pbeWithSHA1AndDES3xCBC_oid)},
        {"PKRoot_oid", static_cast<void *>(&PKRoot_oid)},
        {"SKnew_oid", static_cast<void *>(&SKnew_oid)},
        {"Cert_oid", static_cast<void *>(&Cert_oid)},
        {"FCPath_oid", static_cast<void *>(&FCPath_oid)},
        {"print_cert_flag", compat::print_cert_flag_address()},
        {"print_keyinfo_flag", compat::print_keyinfo_flag_address()},
    };
    return table;
}

} // namespace

using namespace compat;

extern "C" {

void *secude_compat_symbol(const char *name)
{
    if (!name)
        return nullptr;
    for (const auto &s : symbols())
        if (!std::strcmp(s.name, name))
            return s.address;
    return nullptr;
}

int secude_compat_symbol_count(void)
{
    return int(symbols().size());
}

const char *secude_compat_symbol_name(int index)
{
    if (index < 0 || size_t(index) >= symbols().size())
        return nullptr;
    return symbols()[size_t(index)].name;
}

} // extern "C"
