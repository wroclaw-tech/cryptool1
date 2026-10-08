/*
 * Open-source replacement for the subset of the SECUDE SDK used by CrypTool,
 * implemented on top of OpenSSL 3. All SECUDE functions and data objects are
 * exported under their original names (C linkage); this header only declares
 * the additional entry points of the compatibility layer.
 */
#ifndef SECUDE_COMPAT_H
#define SECUDE_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* GetProcAddress()-like lookup of a SECUDE function or data object by its
   undecorated name (e.g. "af_open", "rsa_aid"). Returns NULL if unknown. */
void *secude_compat_symbol(const char *name);

/* Number of entries in the symbol table and access by index (for tests). */
int secude_compat_symbol_count(void);
const char *secude_compat_symbol_name(int index);

/* Human readable identification of this library. */
const char *secude_compat_version(void);

/* 1 if the OpenSSL legacy provider is used for MD4/DES/IDEA/RC2/RC4,
   0 if the built-in implementations are used instead. */
int secude_compat_legacy_provider_active(void);

/* Default PIN of the CrypTool CA PSE (PSEUDO_MASTER_CA_PINNR). */
#define SECUDE_COMPAT_CA_PIN "3.14159265358979323844"
#define SECUDE_COMPAT_CA_NAME "CN=CrypTool CA 2, DC=cryptool, DC=org"

/* Creates a CA PSE (RSA key, self-signed certificate, PKRoot, SerialNo) at
   ca_pse_path and an empty CA database in ca_dir. Returns 0 on success. */
int secude_compat_create_ca(const char *ca_pse_path, const char *ca_dir,
                            const char *pin, const char *ca_name,
                            int key_bits, int validity_days);

/* Creates the complete key store below key_store_root:
     <root>/PSE/PSECA/capse.cse   CA PSE (PIN SECUDE_COMPAT_CA_PIN)
     <root>/PSE/PSECA/cadb.der    CA database
     <root>/PSE/[SideChannelAttack][Bob][RSA-512][1152179494][PIN=1234].pse
     <root>/PSE/[HybridEncryption][Bob][EC-prime239v1][1178702474][PIN=1234](.pse)
   Existing files are left untouched unless overwrite is non-zero.
   Returns 0 on success. */
int secude_compat_create_sample_keystore(const char *key_store_root, int overwrite);

#ifdef __cplusplus
}
#endif

#endif
