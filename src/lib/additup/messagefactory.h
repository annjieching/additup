// ----------------------------------------------------------------------------
// Project: additup
/// @file   messagefactory.h
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
#ifndef MESSAGEFACTORY_H
#define MESSAGEFACTORY_H

// -------------- Includes
// --- C
#include <stdint.h>
// --- C++
#include <string>
#include <list>
// --- Qt
// --- OS
// --- Project lib
// --- Project


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
class TMessage;
class TBitcoinPeer;
class TBitcoinScript;


// -------------- Function pre-class prototypes


// -------------- Class declarations

//
// Class:	TMessageFactory
// Description:
// TMessageFactory builds TMessages from incoming bytes.
//
// TMessageFactorys are owned (typically) by a TBitcoinPeer, which uses
// TVersioningMessageFactory to read initial version messages, and then
// TMessage_version::createMessageFactory() to create a
// TVersionedMessageFactory to read messages.
//
class TMessageFactory
{
  public:
	TMessageFactory();
	virtual ~TMessageFactory();
	virtual const char *className() { return "TMessageFactory"; }

	void receive( const string & );
	void transmit( TMessage * );

	void setPeer( TBitcoinPeer *p ) { Peer = p; }

	string::size_type findNextMagic( const string &, string::size_type = 0 ) const;

	const string &getRXBuffer() const { return RXBuffer; }

  protected:
	virtual void init();

	virtual bool continuousParse() const { return true; }

  protected:
	string RXBuffer;

	bool Initialised;

	list<const TMessage *> Templates;

	TBitcoinPeer *Peer;
};

//
// Class:	TVersioningMessageFactory
// Description:
// TVersioningMessageFactory builds TMessage_version children only.
//
// The special property of TMessage_version children is that they supply
// the createMessageFactory() member function, which creates a
// TVersionedMessageFactory child.  That is to say, that the unversioned
// version message creates the appropriate factory for the version
// specified in that version message.
//
class TVersioningMessageFactory : public TMessageFactory
{
  public:
	TVersioningMessageFactory() : VersionSent(false), VerackSent( false ), VerackReceived( false ) {}

	const char *className() { return "TVersioningMessageFactory"; }

	bool getReady() const { return VerackSent && VerackReceived; }

  protected:
	void init();
	bool continuousParse() const { return false; }

  protected:
	bool VersionSent;
	bool VerackSent;
	bool VerackReceived;
};

//
// Class:	TVersionedMessageFactory
// Description:
// TVersionedMessageFactory builds coherently versioned TMessages
//
// Not all versions of the protocol support all TMessages; and some
// messages have different implementations in different protocol
// versions, TVersionedMessageFactory supplies the appropriate set for a
// given protocol version.
//
// Or rather, it's children do.  By implementing minimumAcceptedVersion()
// and supplying an init() that creates the correct TMessage templates.
//
// If a new message type is added by the bitcoin developers, then it
// gets a TVersionedMessageFactory child to define where it goes, and a
// new TVersion_message child to create that factory.
//
class TVersionedMessageFactory : public TMessageFactory
{
  public:
	const char *className() { return "TVersionedMessageFactory"; }

	virtual TBitcoinScript *createVersionedBitcoinScript() const = 0;

	virtual bool verackRequired() const { return false; }

  protected:
	void init() = 0;

  protected:
	virtual uint32_t minimumAcceptedVersion() const = 0;
};

//
// Class:	TMessageFactory_1
// Description:
//
class TMessageFactory_1 : public TVersionedMessageFactory
{
  public:
	const char *className() { return "TMessageFactory_1"; }

	TBitcoinScript *createVersionedBitcoinScript() const;

  protected:
	void init();

	uint32_t minimumAcceptedVersion() const { return 0; }
};

//
// Class:	TMessageFactory_10600
// Description:
//
class TMessageFactory_10600 : public TVersionedMessageFactory
{
  public:
	const char *className() { return "TMessageFactory_10600"; }

	TBitcoinScript *createVersionedBitcoinScript() const;

  protected:
	void init();

	uint32_t minimumAcceptedVersion() const { return 10600; }
};

//
// Class:	TMessageFactory_20900
// Description:
//
class TMessageFactory_20900 : public TVersionedMessageFactory
{
  public:
	const char *className() { return "TMessageFactory_20900"; }

	TBitcoinScript *createVersionedBitcoinScript() const;

	bool verackRequired() const { return true; }

  protected:
	void init();

	uint32_t minimumAcceptedVersion() const { return 20900; }
};

//
// Class:	TMessageFactory_31402
// Description:
//
class TMessageFactory_31402 : public TVersionedMessageFactory
{
  public:
	const char *className() { return "TMessageFactory_31402"; }

	TBitcoinScript *createVersionedBitcoinScript() const;

	bool verackRequired() const { return true; }

  protected:
	void init();

	uint32_t minimumAcceptedVersion() const { return 31402; }
};


// -------------- Constants


// -------------- Inline Functions


// -------------- Function prototypes


// -------------- Template instantiations


// -------------- World globals ("extern"s only)


// End of conditional compilation
#endif
