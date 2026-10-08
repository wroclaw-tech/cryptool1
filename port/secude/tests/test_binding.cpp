// Compiles CrypTool/SecudeLib.h exactly as the application does and binds
// every member through secude_compat_symbol(), mirroring the non-Windows
// branch of CSecudeLib::OpenSecudeLib().
typedef void *HINSTANCE;
typedef unsigned int UINT;

#include "SecudeLib.h"
#include "secude_compat.h"
#include "testing.hpp"

#include <cstring>

int CSecudeLib::OpenSecudeLib()
{
    int errcnt = 0;
#define DoOneFn(a, b, c, d)                                                                                            \
    c = (c##_t)secude_compat_symbol(#c);                                                                               \
    if (c == NULL)                                                                                                     \
        errcnt++;
#define DoOneData(a, b)                                                                                                \
    b = (a *)secude_compat_symbol(#b);                                                                                 \
    if (b == NULL)                                                                                                     \
        errcnt++;
    DoAll
#undef DoOneFn
#undef DoOneData
    char *line;
    Status = errcnt ? 0 : (SECUDE_HasValidTicket(&line) < 0 ? 1 : 2);
    if ((MaxBits = light_version()) == 0)
        MaxBits = 8192;
    return errcnt;
}

CSecudeLib::CSecudeLib() : MaxBits(0), hDLL(nullptr), Status(0) {}
CSecudeLib::~CSecudeLib() {}
int CSecudeLib::CloseSecudeLib() { return 0; }

TEST(binding_cryptool_secudelib)
{
    CSecudeLib lib;
    CHECK_EQ(lib.OpenSecudeLib(), 0);
    CHECK_EQ(lib.GetStatus(), 2);
    CHECK_EQ(lib.MaxBits, 8192);
    CHECK(secude_compat_symbol("no_such_symbol") == nullptr);

    char data[] = "abc";
    OctetString in = {3, data};
    OctetString out = {0, nullptr};
    CHECK_EQ(lib.sec_hash_all(&in, &out, lib.md5_aid, NULL), 0);
    CHECK_HEX(out.octets, out.noctets, "900150983cd24fb0d6963f7d28e17f72");
    lib.aux_free(out.octets);

    // flags are written through sec_uint4 pointers
    *lib.print_keyinfo_flag = 0x0d;
    *lib.print_cert_flag = 0x77e;
    CHECK_EQ(*lib.print_keyinfo_flag, 0x0du);

    CHECK(lib.af_open(const_cast<char *>("/nonexistent/x.pse"), NULL, const_cast<char *>("1"), NULL) == NULL);
    CHECK(lib.LASTERROR != 0);
    CHECK(std::strlen(lib.LASTTEXT) > 0);
    char *err = lib.aux_sprint_error(NULL, NULL, 1);
    CHECK(err && std::strstr(err, "EPSENOTEXISTING"));
    lib.aux_free_String(&err);
    lib.aux_free_error();
    CHECK_EQ(lib.LASTERROR, 0);
    CHECK_EQ((*lib.Cert_OID)->oid_nelem, lib.Cert_oid->oid_nelem);
    CHECK(lib.rsa_aid->param != nullptr);
    CHECK(std::strstr(lib.aux_sprint_version(NULL), "compatibility") != nullptr);
}
