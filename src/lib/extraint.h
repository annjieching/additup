// ----------------------------------------------------------------------------
// Project: library
/// @file   extraint.h
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
#ifndef EXTRAINT_H
#define EXTRAINT_H

// -------------- Includes
// --- C
// --- C++
#include <vector>
#include <string>
#include <stdexcept>
// --- Qt
// --- OS
// --- Project
// --- Project lib


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
// Class:	TGenericBigInteger
// Description:
/// Arbitrary precision integer.
//
/// Got some help by reading the source for this similar implementation,
/// which wasn't written how I like, but gave me hints to the
/// appropriate algorithms.
///
/// https://mattmccutchen.net/bigint/
//
template <typename tLittleInteger>
class TGenericBigInteger
{
  public:
	enum eComparisonResult {
		LessThan = -1,
		EqualTo = 0,
		GreaterThan = 1
	};
	typedef tLittleInteger tStorageType;
	typedef vector<tLittleInteger> tLittleDigitsVector;
	typedef typename tLittleDigitsVector::size_type tIndex;
	static const size_t bitsPerBlock;

  public:
	TGenericBigInteger() { LittleDigits.push_back(0); }
	TGenericBigInteger( const TGenericBigInteger &O ) { operator=(O); }
	TGenericBigInteger( const string &s, unsigned int b = 10 ) { fromString(s,b); }
	TGenericBigInteger( int t ) { operator=(static_cast<unsigned int>(t) ); }
	TGenericBigInteger( tLittleInteger r0 ) { operator=(r0); }
	TGenericBigInteger( tLittleInteger r1, tLittleInteger r0 );
	TGenericBigInteger( tLittleInteger r2, tLittleInteger r1, tLittleInteger r0 );
	TGenericBigInteger( tLittleInteger r3, tLittleInteger r2, tLittleInteger r1, tLittleInteger r0 );
	TGenericBigInteger( unsigned long long t ) { operator=(t); }

	void invalidate() { LittleDigits.clear(); }
	bool isValid() const { return !LittleDigits.empty(); }
	bool isZero() const { return LittleDigits.size() == 1 && LittleDigits.back() == 0; }

	// Assignment
	TGenericBigInteger &operator=( const string &s ) { return fromString(s, 10); }
//	TGenericBigInteger &operator=( unsigned int t ) { LittleDigits.clear(); LittleDigits.push_back(t); return *this; }
	TGenericBigInteger &operator=( unsigned long long t );
	TGenericBigInteger &operator=( const TGenericBigInteger &O ) { LittleDigits = O.LittleDigits; return *this; }

	// Access
	bool getBit( tIndex bi ) const {
		if( bi/bitsPerBlock < LittleDigits.size() ) {
			return (LittleDigits[bi/bitsPerBlock] & (1 << (bi % bitsPerBlock))) != 0;
		} else {
			return false;
		}
	}
	tLittleInteger getBlock( tIndex bl ) const { return isValid() ? LittleDigits[bl] : 0; }
	tIndex highestBit() const;
	tIndex lowestBit() const;
//	bool setBit( tIndex bi, bool b );

	// Arithmetic - Compound
	TGenericBigInteger &operator +=(const TGenericBigInteger &x);
	TGenericBigInteger &operator -=(const TGenericBigInteger &x);
	TGenericBigInteger &operator *=(const TGenericBigInteger &x);
	TGenericBigInteger &operator /=(const TGenericBigInteger &x) { TGenericBigInteger q; divideWithRemainder(x,q); *this = q; return *this; }
	TGenericBigInteger &operator %=(const TGenericBigInteger &x) { TGenericBigInteger q; divideWithRemainder(x,q); return *this; }
	TGenericBigInteger &operator &=(const TGenericBigInteger &x);
	TGenericBigInteger &operator |=(const TGenericBigInteger &x);
	TGenericBigInteger &operator ^=(const TGenericBigInteger &x);
	TGenericBigInteger &operator <<=( tIndex );
	TGenericBigInteger &operator >>=( tIndex );
	TGenericBigInteger &blockShiftLeft( tIndex );
	TGenericBigInteger &blockShiftRight( tIndex );
	TGenericBigInteger &divideWithRemainder(const TGenericBigInteger &b, TGenericBigInteger &q);

	TGenericBigInteger &operator++() { return (*this += 1);}
	TGenericBigInteger &operator++( int ) { return (*this += 1);}
	TGenericBigInteger &operator--() { return (*this -= 1); }
	TGenericBigInteger &operator--( int ) { return (*this -= 1); }

	// Arithmetic - non compound
	TGenericBigInteger operator+( const TGenericBigInteger &x ) const { return TGenericBigInteger(*this) += x; }
	TGenericBigInteger operator-( const TGenericBigInteger &x ) const { return TGenericBigInteger(*this) -= x; };
	TGenericBigInteger operator*( const TGenericBigInteger &x ) const { return TGenericBigInteger(*this) *= x; };
	TGenericBigInteger operator/( const TGenericBigInteger &x ) const { return TGenericBigInteger(*this) /= x; };
	TGenericBigInteger operator%( const TGenericBigInteger &x ) const { return TGenericBigInteger(*this) %= x; };
	TGenericBigInteger operator&( const TGenericBigInteger &x ) const { return TGenericBigInteger(*this) &= x; };
	TGenericBigInteger operator|( const TGenericBigInteger &x ) const { return TGenericBigInteger(*this) |= x; };
	TGenericBigInteger operator^( const TGenericBigInteger &x ) const { return TGenericBigInteger(*this) ^= x; };
	TGenericBigInteger operator~() const;
	TGenericBigInteger operator<<( tIndex b ) const { return TGenericBigInteger(*this) <<= b; };
	TGenericBigInteger operator>>( tIndex b ) const { return TGenericBigInteger(*this) >>= b; };

	// Comparison
	eComparisonResult compareTo( const TGenericBigInteger & ) const;
	bool operator<( const TGenericBigInteger &O ) const { return isValid() && O.isValid() && compareTo(O) == LessThan; }
	bool operator>( const TGenericBigInteger &O ) const { return isValid() && O.isValid() && compareTo(O) == GreaterThan; }
	bool operator==( const TGenericBigInteger &O ) const { return isValid() && O.isValid() && compareTo(O) == EqualTo; }
	bool operator!=( const TGenericBigInteger &O ) const { return isValid() && O.isValid() && !(*this == O); }
	bool operator<=( const TGenericBigInteger &O ) const { return isValid() && O.isValid() && !(*this > O); }
	bool operator>=( const TGenericBigInteger &O ) const { return isValid() && O.isValid() && !(*this < O); }

	virtual ostream &printOn( ostream & ) const;
	string toString( unsigned int = 10 ) const;

	string toBytes(unsigned int = 0) const;

  protected:
	void normalise();

	TGenericBigInteger &fromString( const string &, unsigned int = 10 );

	virtual unsigned int fromCharacter( unsigned int, unsigned int ) const;
	virtual unsigned int toCharacter( unsigned int, unsigned int ) const;
	virtual string stringPad( const string &, unsigned int ) const;

  protected:
	tLittleDigitsVector LittleDigits;

  private:
	static void blockMultiply(tLittleInteger &R1, tLittleInteger &R0,
		tLittleInteger B, const tLittleInteger C );

};


// -------------- Constants


// -------------- Inline Functions

template <typename tLittleInteger>
inline ostream &operator<<( ostream &s, const TGenericBigInteger<tLittleInteger> &O )
{
	return O.printOn(s);
}


// -------------- Function prototypes


// -------------- Template instantiations
typedef TGenericBigInteger<unsigned int> TBigInteger;



// -------------- World globals ("extern"s only)

// End of conditional compilation
#endif
