/*
 * Copyright 2002-2004 The Apache Software Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * imitations under the License.
 */

/*
 * XSEC
 *
 * OpenSSLCryptoKeyDSA := DSA Keys
 *
 * Author(s): Berin Lautenbach
 *
 * $Id$
 *
 */
#include <xsec/framework/XSECDefs.hpp>
#if defined (HAVE_OPENSSL)

#include <xsec/enc/OpenSSL/OpenSSLCryptoKeyDSA.hpp>
#include <xsec/enc/OpenSSL/OpenSSLCryptoBase64.hpp>
#include <xsec/enc/XSECCryptoException.hpp>
#include <xsec/framework/XSECError.hpp>

OpenSSLCryptoKeyDSA::OpenSSLCryptoKeyDSA() {

	// Create a new key to be loaded as we go

	mp_dsaKey = DSA_new();

};
OpenSSLCryptoKeyDSA::~OpenSSLCryptoKeyDSA() {


	// If we have a DSA, delete it
	// OpenSSL will ensure the memory holding any private key is overwritten

	if (mp_dsaKey)
		DSA_free(mp_dsaKey);

};

// Generic key functions

XSECCryptoKey::KeyType OpenSSLCryptoKeyDSA::getKeyType() {

	// Find out what we have
	if (mp_dsaKey == NULL)
		return KEY_NONE;

	if (mp_dsaKey->priv_key != NULL && mp_dsaKey->pub_key != NULL)
		return KEY_DSA_PAIR;

	if (mp_dsaKey->priv_key != NULL)
		return KEY_DSA_PRIVATE;

	if (mp_dsaKey->pub_key != NULL)
		return KEY_DSA_PUBLIC;

	return KEY_NONE;

}

void OpenSSLCryptoKeyDSA::loadPBase64BigNums(const char * b64, unsigned int len) {

	if (mp_dsaKey == NULL)
		mp_dsaKey = DSA_new();

	mp_dsaKey->p = OpenSSLCryptoBase64::b642BN((char *) b64, len);

}

void OpenSSLCryptoKeyDSA::loadQBase64BigNums(const char * b64, unsigned int len) {

	if (mp_dsaKey == NULL)
		mp_dsaKey = DSA_new();

	mp_dsaKey->q = OpenSSLCryptoBase64::b642BN((char *) b64, len);

}

void OpenSSLCryptoKeyDSA::loadGBase64BigNums(const char * b64, unsigned int len) {

	if (mp_dsaKey == NULL)
		mp_dsaKey = DSA_new();

	mp_dsaKey->g = OpenSSLCryptoBase64::b642BN((char *) b64, len);

}

void OpenSSLCryptoKeyDSA::loadYBase64BigNums(const char * b64, unsigned int len) {

	if (mp_dsaKey == NULL)
		mp_dsaKey = DSA_new();

	mp_dsaKey->pub_key = OpenSSLCryptoBase64::b642BN((char *) b64, len);

}

void OpenSSLCryptoKeyDSA::loadJBase64BigNums(const char * b64, unsigned int len) {

	if (mp_dsaKey == NULL)
		mp_dsaKey = DSA_new();

	// Do nothing
}


// "Hidden" OpenSSL functions

OpenSSLCryptoKeyDSA::OpenSSLCryptoKeyDSA(EVP_PKEY *k) {

	// Create a new key to be loaded as we go

	mp_dsaKey = DSA_new();
	
	if (k == NULL || k->type != EVP_PKEY_DSA)
		return;	// Nothing to do with us


	if (k->pkey.dsa->p)
		mp_dsaKey->p = BN_dup(k->pkey.dsa->p);
	if (k->pkey.dsa->q)
		mp_dsaKey->q = BN_dup(k->pkey.dsa->q);
	if (k->pkey.dsa->g)
		mp_dsaKey->g = BN_dup(k->pkey.dsa->g);
	if (k->pkey.dsa->pub_key)
		mp_dsaKey->pub_key = BN_dup(k->pkey.dsa->pub_key);
	if (k->pkey.dsa->priv_key)
		mp_dsaKey->priv_key = BN_dup(k->pkey.dsa->priv_key);

}

// --------------------------------------------------------------------------------
//           Verify a signature encoded as a Base64 string
// --------------------------------------------------------------------------------

bool OpenSSLCryptoKeyDSA::verifyBase64Signature(unsigned char * hashBuf, 
								 unsigned int hashLen,
								 char * base64Signature,
								 unsigned int sigLen) {

	// Use the currently loaded key to validate the Base64 encoded signature

	if (mp_dsaKey == NULL) {

		throw XSECCryptoException(XSECCryptoException::DSAError,
			"OpenSSL:DSA - Attempt to validate signature with empty key");
	}

	unsigned char sigVal[512];
	int sigValLen;
	int err;

	EVP_ENCODE_CTX m_dctx;
	int rc;

	EVP_DecodeInit(&m_dctx);
	rc = EVP_DecodeUpdate(&m_dctx, 
						  sigVal, 
						  &sigValLen, 
						  (unsigned char *) base64Signature, 
						  sigLen);

	if (rc < 0) {

		throw XSECCryptoException(XSECCryptoException::DSAError,
			"OpenSSL:DSA - Error during Base64 Decode");
	}
	int t = 0;

	EVP_DecodeFinal(&m_dctx, &sigVal[sigValLen], &t); 

	sigValLen += t;

	if (sigValLen != 40) {

		throw XSECCryptoException(XSECCryptoException::DSAError,
			"OpenSSL:DSA - Signature Length incorrect");
	}

	// Translate to BNs and thence to DSA_SIG
	BIGNUM * R = BN_bin2bn(sigVal, 20, NULL);
	BIGNUM * S = BN_bin2bn(&sigVal[20], 20, NULL);

	DSA_SIG * dsa_sig = DSA_SIG_new();

	dsa_sig->r = BN_dup(R);
	dsa_sig->s = BN_dup(S);

	BN_free(R);
	BN_free(S);

	unsigned char sigValTranslatedBuf[256];
	unsigned char * sigValTranslated = sigValTranslatedBuf;
	int sigValTranslatedLen;

	sigValTranslatedLen = i2d_DSA_SIG(dsa_sig, &sigValTranslated);

	// Now we have a signature and a key - lets check
	
	err = DSA_do_verify(hashBuf, hashLen, dsa_sig, mp_dsaKey);

	DSA_SIG_free(dsa_sig);

	if (err < 0) {

		throw XSECCryptoException(XSECCryptoException::DSAError,
			"OpenSSL:DSA - Error validating signature");
	}

	return (err == 1);

}

// --------------------------------------------------------------------------------
//           Sign and encode result as a Base64 string
// --------------------------------------------------------------------------------


unsigned int OpenSSLCryptoKeyDSA::signBase64Signature(unsigned char * hashBuf,
		unsigned int hashLen,
		char * base64SignatureBuf,
		unsigned int base64SignatureBufLen) {

	// Sign a pre-calculated hash using this key

	if (mp_dsaKey == NULL) {

		throw XSECCryptoException(XSECCryptoException::DSAError,
			"OpenSSL:DSA - Attempt to sign data with empty key");
	}

	DSA_SIG * dsa_sig;

	dsa_sig = DSA_do_sign(hashBuf, hashLen, mp_dsaKey);

	if (dsa_sig == NULL) {

		throw XSECCryptoException(XSECCryptoException::DSAError,
			"OpenSSL:DSA - Error signing data");

	}

	// Now turn the signature into a base64 string

	unsigned char rawSigBuf[256];
	unsigned int rawLen;

	rawLen = BN_bn2bin(dsa_sig->r, rawSigBuf);
	
	if (rawLen <= 0) {

		throw XSECCryptoException(XSECCryptoException::DSAError,
			"OpenSSL:DSA - Error converting signature to raw buffer");

	}

	unsigned int rawLenS = BN_bn2bin(dsa_sig->s, (unsigned char *) &rawSigBuf[rawLen]);

	if (rawLenS <= 0) {

		throw XSECCryptoException(XSECCryptoException::DSAError,
			"OpenSSL:DSA - Error converting signature to raw buffer");

	}

	rawLen += rawLenS;

	// Now convert to Base 64

	BIO * b64 = BIO_new(BIO_f_base64());
	BIO * bmem = BIO_new(BIO_s_mem());

	BIO_set_mem_eof_return(bmem, 0);
	b64 = BIO_push(b64, bmem);

	// Translate signature from Base64

	BIO_write(b64, rawSigBuf, rawLen);
	BIO_flush(b64);

	unsigned int sigValLen = BIO_read(bmem, base64SignatureBuf, base64SignatureBufLen);

	BIO_free_all(b64);

	if (sigValLen <= 0) {

		throw XSECCryptoException(XSECCryptoException::DSAError,
			"OpenSSL:DSA - Error base64 encoding signature");
	}

	return sigValLen;

}



XSECCryptoKey * OpenSSLCryptoKeyDSA::clone() {

	OpenSSLCryptoKeyDSA * ret;

	XSECnew(ret, OpenSSLCryptoKeyDSA);

	ret->m_keyType = m_keyType;
	ret->mp_dsaKey = DSA_new();

	// Duplicate parameters
	if (mp_dsaKey->p)
		ret->mp_dsaKey->p = BN_dup(mp_dsaKey->p);
	if (mp_dsaKey->q)
		ret->mp_dsaKey->q = BN_dup(mp_dsaKey->q);
	if (mp_dsaKey->g)
		ret->mp_dsaKey->g = BN_dup(mp_dsaKey->g);
	if (mp_dsaKey->pub_key)
		ret->mp_dsaKey->pub_key = BN_dup(mp_dsaKey->pub_key);
	if (mp_dsaKey->priv_key)
		ret->mp_dsaKey->priv_key = BN_dup(mp_dsaKey->priv_key);

	return ret;

}

#endif /* HAVE_OPENSSL */