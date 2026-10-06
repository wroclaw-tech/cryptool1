// Source-compatible replacement for the cv act library (subset used by CrypTool).
#ifndef ACT_STATE_H
#define ACT_STATE_H

namespace act
{
	const int READY = 0;
	const int SIGNATURE_OK = 1;
	const int IS_FINALIZED = 2;
	const int DECRYPT_ERROR = 3;
	const int CERTIFICATE_OK = 4;
	const int CERTIFICATE_ERROR = 5;
	const int VERIFY_ERROR = 6;
} // namespace act

#endif // ACT_STATE_H
