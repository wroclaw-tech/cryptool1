# secude_compat — SECUDE replacement for the CrypTool port

CrypTool 1 on Windows loads the proprietary `SECUDE.DLL`. This static library
implements the subset of the SECUDE SDK 7.4 API that CrypTool and libec use
(every function and data object of the `DoAll` list in
`CrypTool/SecudeLib.h`, plus `global_add_error` and `rabinstest`, which libec
calls directly) on top of OpenSSL 3. The SECUDE headers in `secude/` are used
unchanged apart from small portability guards.

## Building

```
cmake -S port/secude -B build/secude -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3
cmake --build build/secude && ctest --test-dir build/secude
```

Targets: `secude_compat` (static library; public include directories `secude/`
and `port/secude/include`, links `OpenSSL::Crypto`), `secude_compat_mkpse`
(key store generator) and, when `SECUDE_COMPAT_TESTS` is on (default for
standalone builds), `secude_compat_tests`. ctest runs the suite twice: with
the OpenSSL legacy provider and with `SECUDE_COMPAT_NO_LEGACY_PROVIDER=1`.

## Binding

* `CrypTool/SecudeLib.cpp`: on non-Windows platforms `OpenSecudeLib()` fills
  the `CSecudeLib` function/data pointers through `secude_compat_symbol(name)`
  (a `GetProcAddress` replacement) instead of loading `SECUDE.DLL`.
* `libec/sources/ECsecude.c`: on non-Windows platforms `ECSecudeLib` is
  statically initialised with the library functions, so libec works even
  before `CrypToolApp` copies the pointers.

## Algorithms

| AlgId | Implementation |
| --- | --- |
| `md2_aid`, `sha_aid` (SHA-0) | built in (not available in OpenSSL 3) |
| `md4_aid`, `ripemd160_aid` | OpenSSL (legacy/default provider), built-in fallback |
| `md5_aid`, `sha1_aid` | OpenSSL |
| `desECB_aid`, `desCBC_pad_aid`, `desEDE_aid`, `desCBC3_aid`, `idea_aid`, `rc2CBC_aid`, `rc4_aid` | OpenSSL legacy provider, built-in fallback |
| `rsa_aid` (2.5.8.1.1, INTEGER keysize), `rsaEncryption_aid` | OpenSSL BIGNUM |
| `dsa_aid` (1.3.14.3.2.12, Dss-Parms) | OpenSSL BIGNUM |

The legacy provider is loaded into a private `OSSL_LIB_CTX`; whatever it
does not offer (all of it if it is missing, IDEA on Debian) is served by the
built-in implementations, which the tests cross-check against OpenSSL.

Notes on behaviour that CrypTool relies on:

* Hash results are always written to freshly allocated `octets`; input and
  output may be the same `OctetString`.
* Symmetric output and RSA en/decryption output are written into the caller's
  buffer (`bits`/`octets`) when it is non-NULL (appending at `nbits`/`noctets`),
  otherwise allocated.
* SECUDE "std" padding (`desECB`, `desCBC`, `desEDE`, `desCBC3`, `idea`): the
  last block is filled with zero octets, its last octet holds the number of
  data octets in that block. `desCBC_pad`, `desCBC3_pad`, `desEDE3CBC`,
  `ideaCBC`, `rc2CBC` and AES use PKCS#5 padding. IVs default to zero; RC2
  uses the key length as effective key bits unless the parameter says
  otherwise; `idea_aid` is ECB.
* Bad padding fails with `EDECRYPTION` (1792), which the brute-force analysis
  treats as "wrong key".
* RSA encryption is textbook RSA on blocks of (bits-1)/8 octets (the last
  block is zero padded on the right), each encrypted into (bits+7)/8 octets;
  decryption yields (bits-1)/8 octets per block. This is what the hybrid
  encryption and side-channel demos expect.
* RSA signatures are PKCS#1 v1.5 with the DigestInfo encodings listed in
  `CrypTool/PSEDemo.cpp`; DSA signatures are DER `SEQUENCE { r, s }`.
* RSA private keys are stored like SECUDE does: `KeyBits { p, q }`. The public
  exponent is taken from the PSE certificate, 65537 otherwise.
* `L_NUMBER` is the 32-bit `sec_uint4` of the headers on every platform:
  `a[0]` holds the number of words (two's complement for negative values),
  `a[1..n]` the magnitude, least significant word first.

## File formats

### PSE (`*.pse`, CA: `capse.cse`)

DER encoded, encrypted with AES-256-GCM under a PBKDF2-HMAC-SHA256 key
(100000 iterations, 16 octet salt) derived from the PIN:

```
CompatPSE ::= SEQUENCE {
    format     UTF8String "SECUDE-compat PSE",
    version    INTEGER (1),
    kdf        SEQUENCE { pbkdf2 OID, salt OCTET STRING, iterations INTEGER, hmacWithSHA256 OID },
    cipher     SEQUENCE { aes256-GCM OID, nonce OCTET STRING },
    ciphertext OCTET STRING }        -- ciphertext || tag, AAD = DER of the fields above
PSEContents ::= SEQUENCE { flags INTEGER (1 = one key pair only), created Time,
                           objects SEQUENCE OF PSEObject }
PSEObject   ::= SEQUENCE { name UTF8String, type OID, created Time, updated Time,
                           value OCTET STRING }
```

A wrong PIN fails with `EPIN`, a file in another format with `EPSEFILE`.
Object types use the private arc `2.206.5.1` (`SKnew` .1, `SKold` .2,
`SignSK` .3, `DecSKnew` .4, `DecSKold` .5, `AuthSK` .6, `Cert` .10,
`SignCert` .11, `EncCert` .12, `AuthCert` .13, `PKRoot` .20, `FCPath` .21,
`SerialNumber` .30, `Name` .31, `Uid` .40). Values: certificates as X.509
DER, keys as `KeyInfo` (SubjectPublicKeyInfo layout), `PKRoot` and `FCPath`
as below, anything else (e.g. `EcPrivKey` of type `Uid`) as raw octets.

```
PKRoot ::= SEQUENCE { ca Name, newkey SerialKey, oldkey [0] SerialKey OPTIONAL,
                      extensions [3] Extensions OPTIONAL }
SerialKey ::= SEQUENCE { serial INTEGER, version INTEGER, key SubjectPublicKeyInfo,
                         validity Validity OPTIONAL,
                         signature [1] SEQUENCE { AlgorithmIdentifier, BIT STRING } OPTIONAL }
FCPath ::= SEQUENCE OF SET OF Certificate
```

### CA database (`<cadir>/cadb.der`)

```
CADatabase ::= SEQUENCE { version INTEGER (1), entries SEQUENCE OF
                 SEQUENCE { keyType INTEGER, issued Time, certificate Certificate } }
```

Users are looked up by subject DN (case-insensitive, whitespace-normalised
comparison), certificates by serial number. Name strings are written least
significant RDN first (`CN=..., DC=cryptool, DC=org`) as in SECUDE.

## Key store

`secude_compat_mkpse <root>` (or `secude_compat_create_sample_keystore()`)
creates the files the installer ships, in the layout CrypTool expects:

```
<root>/PSE/PSECA/capse.cse   CA PSE "CN=CrypTool CA 2, DC=cryptool, DC=org",
                             RSA-2048, PIN 3.14159265358979323844
<root>/PSE/PSECA/cadb.der    CA database
<root>/PSE/[SideChannelAttack][Bob][RSA-512][1152179494][PIN=1234].pse
<root>/PSE/[HybridEncryption][Bob][EC-prime239v1][1178702474][PIN=1234]      public EC data (text)
<root>/PSE/[HybridEncryption][Bob][EC-prime239v1][1178702474][PIN=1234].pse  EcPrivKey
```

The keys are newly generated (the SECUDE-encrypted originals cannot be read);
names, DNs and PINs match the originals. The EC text file has the format
written by `CKeyFile::CreateEcKeyFiles` (a, b, p, Gx, Gy, cofactor, order,
Qx, Qy as `0X...` hex lines). `secude_compat_create_ca()` creates only the
CA (for a first-run setup).

## Not supported

Smart cards (`SC_DATA` is ignored), revocation lists
(`af_pse_get_CrlSet` fails with `ENOCRL`), subject alternative names in
`af_create_Certificate` and PKCS#8 attributes in `aux_create_PrivateKeyInfo`
(both fail with `ENOTSUPPORTED`), and the SQMODN hash (`HashInput` is ignored).

## Integration notes

* Do not include OpenSSL headers and SECUDE headers in the same translation
  unit: SECUDE's `RSA`, `DSA`, `SHA1`, ... enumerators and its BIO/SSL
  declarations clash with OpenSSL (see `src/internal.hpp` for the renaming
  this library uses internally).
* `CrypTool/SecudeLib.h` uses `register` in its prototypes: fine in C++14
  (deprecation warning), an error in C++17 unless `-Wno-register` is used.
* `CrypToolApp.cpp` builds the key store paths with backslashes
  (`PSE\\PSECA\\capse.cse`); use `/` on other platforms. Its
  `ECSecudeLib.##c = SecudeLib.##c` loop is not valid outside MSVC and can be
  dropped there (`ECsecude.c` already initialises the table).
