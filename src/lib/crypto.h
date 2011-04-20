// ----------------------------------------------------------------------------
// Project: bitcoin
/// @file   crypto.h
/// @author Andy Parkins
//
// Version Control
//    $Author$
//      $Date$
//        $Id$
//
// Legal
//    Copyright 2011  Andy Parkins
//
// ----------------------------------------------------------------------------

// Catch multiple includes
#ifndef CRYPTO_H
#define CRYPTO_H

// -------------- Includes
// --- C
// --- C++
#include <string>
// --- OS
// --- Lib
#include <openssl/objects.h>
#include <openssl/ecdsa.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
// --- Project
#include "extraexcept.h"


// -------------- Namespace
	// --- Imported namespaces
	using namespace std;


// -------------- Defines
// General
// Project


// -------------- Constants


// -------------- Typedefs (pre-structure)


// -------------- Enumerations


// -------------- Structures/Unions


// -------------- Typedefs (post-structure)


// -------------- Class pre-declarations


// -------------- Function pre-class prototypes


// -------------- Class declarations

//
// Class:	ssl_error
// Description:
/// Exception class for ssl library errors
//
class ssl_error : public libc_error
{
  protected:
	SSL *SSLHandle;
	int SSLerror;
	unsigned long SSLQueuedError;
	int ret;
  public:
	// First form of constructor is for errors generated by
	// TSecureSocketConnection.
	explicit ssl_error( const string &s ) throw() :
		libc_error(s, 0), SSLHandle(NULL), SSLerror(SSL_ERROR_NONE), ret(0) {
			SSLQueuedError = ERR_get_error();
		};
	// Second form of constructor is for errors generated by
	// OpenSSL itself
	explicit ssl_error( SSL *h, int r, const string &s ) throw() :
		libc_error(s), SSLHandle(h), ret(r) {
			SSLerror = SSL_get_error(SSLHandle, ret);
			SSLQueuedError = ERR_get_error();
		}

	virtual ~ssl_error() throw() {};

	SSL *getHandle() const { return SSLHandle; }
	int getSSLError() const { return SSLerror; }

	virtual const char* what() const throw();
};


//
// Class:	TEllipticCurveKey
// Description:
//
class TEllipticCurveKey
{
  public:
	TEllipticCurveKey() {
		Key = EC_KEY_new_by_curve_name( NID_secp256k1 );
		EC_KEY_generate_key( Key );
	}
	~TEllipticCurveKey() { EC_KEY_free( Key ); }

	unsigned int getSize() const {
		return ECDSA_size( Key );
	}

	string sign( const string &s ) const {
		string Out;
		unsigned int OutLen = getSize();
		unsigned char *buffer = new unsigned char[OutLen];
		ECDSA_sign( 0,
				reinterpret_cast<const unsigned char *>(s.data()), s.size(),
				buffer, &OutLen,
				Key);
		Out.assign( reinterpret_cast<const char*>(buffer), OutLen );
		delete[] buffer;
		return Out;
	}

	bool verify( const string &digest, const string &signature ) const {
		int ret;
		ret = ECDSA_verify( 0,
				reinterpret_cast<const unsigned char *>(digest.data()), digest.size(),
				reinterpret_cast<const unsigned char *>(signature.data()), signature.size(),
				Key);
		return ret == 1;
	}

  protected:
	EC_KEY *Key;
};

// ------------

//
// Class:		TMessageDigest
// Description:
/// Abstract base class to make a common interface to hashes.
//
/// "Message digest" is the name that OpenSSL (and other cyrptographers)
/// give to what is commonly called a "hash".  The idea is that an
/// abritrary length of arbitrary data has some operation performed on it
/// such that a number is generated.  That number is a cryptographicly
/// robust representation of that data.  The term "cryptographically
/// robust" is used here to mean that it is very difficult to create a
/// set of data that will generate any particular hash.  In other words,
/// we may rely on the hash as representing that particular combination
/// of input data in a way that is not easily faked.
///
/// A good hash is highly sensitive to small changes in the input data,
/// is unpredicatable, and fast.
///
//
class TMessageDigest
{
  public:
	TMessageDigest() : Initialised( false ) {}
	virtual ~TMessageDigest() {}
	virtual string transform( const string & ) = 0;

  protected:
	virtual void init() { Initialised = true; }
	virtual void deinit() { Initialised = false; }

  protected:
	bool Initialised;
};

//
// Class:		TSSLMessageDigest
// Description:
/// Wrapper class around an OpenSSL EVP message digest collection.
//
/// This class represents a convenience wrapper around OpenSSL's message
/// digest functions.  Combined with TSSLMessageDigestTemplate below, it
/// makes hashing data as simple as:
///
///   \code
///   THash_sha1 Hasher;
///   string output = Hasher.transform( data_to_be_hashed );
///   \endcode
///
/// You will find the full list of available hashes at the end of this
/// file in the form:
///
///   \code
///   TEMPLATE_INSTANCE(TSSLMessageDigestTemplate<EVP_md5>, THash_md5);
///   \endcode
///
/// They are template instantiations to make objects per hash type.
//
class TSSLMessageDigest : public TMessageDigest
{
  public:
	TSSLMessageDigest();
	TSSLMessageDigest( const TSSLMessageDigest & );
	virtual ~TSSLMessageDigest();

	string transform( const string & );
	void update( const string & );
	string final();

  protected:
	virtual const EVP_MD *getMD() = 0;
	void init();

  protected:
	EVP_MD_CTX EVPContext;
};

//
// Template:		TSSLMessageDigestTemplate
// Description:
/// Template to generate the various hash classes
//
template < const EVP_MD *(*F)() >
class TSSLMessageDigestTemplate : public TSSLMessageDigest
{
  public:
	TSSLMessageDigestTemplate() : TSSLMessageDigest() {}

  protected:
	const EVP_MD *getMD() { return F(); }
};


// -------------- Constants


// -------------- Inline Functions


// -------------- Function prototypes


// -------------- Template instantiations

#define TEMPLATE_INSTANCE( fulltype, shorttype ) \
	extern template class fulltype; \
	typedef fulltype shorttype;
//
// This block is pulled out of the openssl/evp.h file, but manipulated
// to make the hash classes I want
//
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_md_null>, THash_md_null );
#ifndef OPENSSL_NO_MD2
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_md2>, THash_md2 );
#endif
#ifndef OPENSSL_NO_MD4
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_md4>, THash_md4 );
#endif
#ifndef OPENSSL_NO_MD5
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_md5>, THash_md5 );
#endif
#ifndef OPENSSL_NO_SHA
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_sha>, THash_sha );
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_sha1>, THash_sha1 );
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_dss>, THash_dss );
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_dss1>, THash_dss1 );
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_ecdsa>, THash_ecdsa );
#endif
#ifndef OPENSSL_NO_SHA256
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_sha224>, THash_sha224 );
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_sha256>, THash_sha256 );
#endif
#ifndef OPENSSL_NO_SHA512
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_sha384>, THash_sha384 );
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_sha512>, THash_sha512 );
#endif
#ifndef OPENSSL_NO_MDC2
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_mdc2>, THash_mdc2 );
#endif
#ifndef OPENSSL_NO_RIPEMD
TEMPLATE_INSTANCE( TSSLMessageDigestTemplate<EVP_ripemd160>, THash_ripemd160 );
#endif
#undef TEMPLATE_INSTANCE


// -------------- World globals ("extern"s only)

// End of conditional compilation
#endif
