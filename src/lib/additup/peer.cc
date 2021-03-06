// ----------------------------------------------------------------------------
// Project: additup
/// @file   peer.cc
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

// Module include
#include "peer.h"

// -------------- Includes
// --- C
// --- C++
#include <sstream>
#include <memory>
#include <stdexcept>
// --- Qt
// --- OS
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
// --- Project libs
#include <general/logstream.h>
// --- Project
#include "messagefactory.h"
#include "messages.h"
#include "messageelements.h"
#include "constants.h"


// -------------- Namespace


// -------------- Module Globals


// -------------- World Globals (need "extern"s in header)


// -------------- Template instantiations


// -------------- Class declarations

//
// Function:	TNodeInfo :: TNodeInfo
// Description:
//
TNodeInfo::TNodeInfo() :
	IPv4( 0 ),
	Port( 0 ),
	LastConnectAttempt(0),
	LastConnectSuccess(0)
{
}

//
// Function:	TNodeInfo :: TNodeInfo
// Description:
//
TNodeInfo::TNodeInfo( uint32_t ip ) :
	IPv4( ip ),
	Port( 0 ),
	LastConnectAttempt(0),
	LastConnectSuccess(0)
{
}

//
// Function:	TNodeInfo :: write
// Description:
//
ostream &TNodeInfo::write( ostream &os ) const
{
	// Big endian
	os.put((IPv4 & 0xff000000) >> 24);
	os.put((IPv4 & 0xff0000) >> 16);
	os.put((IPv4 & 0xff00) >> 8);
	os.put((IPv4 & 0xff) >> 0);

	return os;
}

//
// Function:	TNodeInfo :: toSockAddr
// Description:
//
void TNodeInfo::toSockAddr( struct sockaddr &SA ) const
{
	sockaddr_in &SAI( reinterpret_cast<sockaddr_in&>( SA ) );

	memset( &SAI, 0, sizeof( SAI ) );
	SAI.sin_family = PF_INET;
	SAI.sin_port = htons( Port );
	SAI.sin_addr.s_addr = htonl( IPv4 );
}

//
// Function:	TNodeInfo :: printOn
// Description:
//
ostream &TNodeInfo::printOn( ostream &os ) const
{
	os << ((IPv4 & 0xff000000) >> 24)
		<< "." << ((IPv4 & 0xff0000) >> 16)
		<< "." << ((IPv4 & 0xff00) >> 8)
		<< "." << ((IPv4 & 0xff) >> 0)
		<< ":" << Port
		<< " " << LastConnectAttempt << ", " << LastConnectSuccess;

	return os;
}

//
// Function:	TNodeInfo :: get
// Description:
//
string TNodeInfo::get() const
{
	ostringstream oss;

	write(oss);

	return oss.str();
}

// --------

//
// Function:	TBitcoinPeer :: TBitcoinPeer
// Description:
//
TBitcoinPeer::TBitcoinPeer( const TNodeInfo *info, TBitcoinNetwork *network ) :
	Info( info ),
	Network( network ),
	Factory( NULL ),
	State( Unconnected ),
	VersionSent( false ),
	VerackReceived( false ),
	VersionMessage( NULL )
{
}

//
// Function:	TBitcoinPeer :: ~TBitcoinPeer
// Description:
//
TBitcoinPeer::~TBitcoinPeer()
{
	delete VersionMessage;
	delete Factory;

	// Tidy up anything left on the queues
	while( !IncomingQueue.empty() ) {
		delete IncomingQueue.front();
		IncomingQueue.erase( IncomingQueue.begin() );
	}
	while( !OutgoingQueue.empty() ) {
		delete OutgoingQueue.front();
		OutgoingQueue.erase( OutgoingQueue.begin() );
	}
}

//
// Function:	TBitcoinPeer :: getNetworkParameters
// Description:
//
const TNetworkParameters *TBitcoinPeer::getNetworkParameters() const
{
	if( Network == NULL )
		return NULL;
	return Network->getNetworkParameters();
}

//
// Function:	TBitcoinPeer :: oldestIncoming
// Description:
//
TMessage *TBitcoinPeer::oldestIncoming() const
{
	if( IncomingQueue.empty() )
		return NULL;
	return IncomingQueue.front();
}

//
// Function:	TBitcoinPeer :: nextIncoming
// Description:
//
TMessage *TBitcoinPeer::newestIncoming() const
{
	if( IncomingQueue.empty() )
		return NULL;
	return IncomingQueue.back();
}

//
// Function:	TBitcoinPeer :: nextIncoming
// Description:
//
TMessage *TBitcoinPeer::nextIncoming()
{
	if( IncomingQueue.empty() )
		return NULL;
	TMessage *x = IncomingQueue.front();
	IncomingQueue.pop_front();
	return x;
}

//
// Function:	TBitcoinPeer :: nextOutgoing
// Description:
//
TMessage *TBitcoinPeer::nextOutgoing()
{
	if( OutgoingQueue.empty() )
		return NULL;
	TMessage *x = OutgoingQueue.front();
	OutgoingQueue.pop_front();
	return x;
}

//
// Function:	TBitcoinPeer :: queueOutgoing
// Description:
//
void TBitcoinPeer::queueOutgoing( TMessage *m )
{
	// If it's on our queue it's going to this peer
	m->setPeer( this );
	// Make sure the message is valid
	m->setFields();
	OutgoingQueue.push_back( m );
}

//
// Function:	TBitcoinPeer :: queueIncoming
// Description:
//
void TBitcoinPeer::queueIncoming( TMessage *m )
{
	// If it's on our queue it came from our peer
	m->setPeer( this );
	IncomingQueue.push_back( m );
}

//
// Function:	TBitcoinPeer :: receive
// Description:
// receive() passes incoming bytes to the appropriate factory, creating
// factories as necessary.
//
// When unconnected there is no factory.  Once connected, we could be
// talking to any version of node, so we use TVersioningMessageFactory
// to read whatever TMessage_version is sent to us.  TMessage_versions
// are special because they know how to create a factory appropriate to
// the version the message specifies.  Once a TMessage_version is
// received, that factory replaced our TVersioningMessageFactory, and we
// are in full communication.
//
//
void TBitcoinPeer::receive( const TByteArray &s )
{
	TByteArray incoming(s);

	if( State == Unconnected ) {
		log() << "[PEER] State: Unconnected" << endl;
		// Can't receive anything until we at least have comms
		return;
	}

	if( State == Connecting ) {
		log() << "[PEER] State: Connecting" << endl;
		if( Factory != NULL )
			throw logic_error( "TBitcoinPeer::receive() can't have a factory while unconnected" );

		VersionSent = false;
		VerackReceived = false;

		// The versioning factory only understands version messages
		delete Factory;
		Factory = new TVersioningMessageFactory;

		// The TVersioningMessageFactory is told we are the peer it
		// should use; TVersioningMessageFactory tells its created
		// TMessage_versions that peer; TMessage_versions tell their
		// created TVersionedMessageFactorys that peer.  Hence we don't
		// need to do anything other than this initial setPeer() call.
		Factory->setPeer( this );

		// We're now checking we know what messages look like
		State = Parameters;
	}

	if( State == Parameters ) {
		// This should only need doing for the very first peer;
		// subsequent peers will look at the network and find the
		// network parameters are already set, and will use those.
		log() << "[PEER] State: Parameters" << endl;
		if( Network != NULL && getNetworkParameters() != NULL ) {
			log() << "[PEER] Network parameters already available, "
				<< getNetworkParameters()->className() << endl;
			State = Handshaking;
		} else {
			log() << "[PEER] Network parameters not available, queueing messages with all known magics" << endl;
			// If we don't already know our network, then the first thing
			// we're looking for is a magic number to tell us what network
			// we're connected to.  We can't transmit anything until we
			// know that.

			// We will make the assumption that the accidental
			// transmission of bytes matching a network magic number is
			// impossible (or at least 2^32 to 1 against).  Therefore
			// we'll look through a window until we see a match.

			// Try reading four bytes starting from each byte in turn
			istringstream iss(incoming);
			while( iss.good() ) {
				TLittleEndian32Element PotentialMagic;
				streamoff pos;
				iss.exceptions( ios::eofbit | ios::failbit | ios::badbit );

				// Bookmark
				pos = iss.tellg();

				// Attempt read
				try {
					PotentialMagic.read(iss);
				} catch( ... ) {
					break;
				}

				// Restore
				iss.seekg(static_cast<streamoff>(pos+1));

				// Compare magic against all known networks
				log() << "[PEER]  - Testing potential network magic " << hex << PotentialMagic.getValue() << dec << endl;

				const TNetworkParameters *p = KNOWN_NETWORKS::O().magicToNetwork( PotentialMagic.getValue() );
				if( p == NULL ) {
					if( Network != NULL ) {
						log() << "[PEER]  - Network magic found, for " << p->networkName() << endl;
						Network->setNetworkParameters( p );
					} else {
						log() << "[PEER]  - Network magic for " << p->networkName() << " will not be used" << endl;
					}
					State = Handshaking;
					break;
				}
			}

			// Send some version messages -- the correct magic will get us
			// an answer, the wrong ones will be ignored
			// XXX: This is dodgy.  What if the other end is doing this
			// sort of auto-detection as well?
			if( incoming.empty() ) {
				KNOWN_NETWORKS::T::const_iterator p = KNOWN_NETWORKS::O().begin();
				while( p != KNOWN_NETWORKS::O().end() ) {
					TMessage_version *version;
					version = Network->createMyVersionMessage();
					version->setFields();
					version->setMagic( (*p)->Magic );
					queueOutgoing( version );
					p++;
				}
				// We've definitely sent our version now
				VersionSent = true;
			}
		}
	}

	if( State == Handshaking ) {
		log() << "[PEER] State: Handshaking" << endl;
		if( Factory == NULL )
			throw logic_error( "TBitcoinPeer::receive() must have factory in handshaking mode" );

		// Send our version
		if( !VersionSent ) {
			VersionSent = true;
			queueOutgoing( Network->createMyVersionMessage() );
		}

		// Convert stream to messages
		try {
			Factory->receive(incoming);
			incoming.clear();
		} catch( exception &e ) {
			log() << "[PEER] Error parsing message, " << e.what() << endl;
			return;
		}

		while( true ) {
			auto_ptr<TMessage> Message( nextIncoming() );

			if( dynamic_cast<TMessage_version*>( Message.get() ) != NULL ) {
				if( VersionMessage == NULL ) {
					// We don't care exactly what version message, the
					// TMessage_version base class is more than enough for us to
					// query the message
					VersionMessage = reinterpret_cast<TMessage_version*>( Message.get()->clone() );

					log() << "[PEER] Version message received, " << *VersionMessage << endl;
					TVersionedMessageFactory *newFactory = VersionMessage->createMessageFactory();
					log() << "[PEER] Factory is now " << newFactory->className() << endl;
					// Any bytes left over in the factory we're about to delete
					// must be forwarded to the new factory
					newFactory->receive( Factory->getRXBuffer() );
					delete Factory;
					Factory = newFactory;

					// Some versions don't require verack, so we need to
					// check for that and bypass that requirement
					if( !newFactory->verackRequired() )
						VerackReceived = true;
				}

				// We'll leave acknowledgement to the network

				// Requeue the version message so the network gets to see it
				// once we get to the "Connected" stage
				queueIncoming( Message.get() );
				// ... since we've requeued it, don't delete it
				Message.release();

				// If the requeued version is the only thing on the
				// queue, then we're done with the loop
				if( IncomingQueue.size() == 1 )
					break;

			} else if( dynamic_cast<TMessage_verack*>( Message.get() ) != NULL ) {
				// Not sure we care...  If we don't get a verack, then
				// presumably the remote will just hang up on us -- what
				// else can it do?
				VerackReceived = true;
				log() << "[PEER] Version acknowledgement received" << endl;
			} else {
				// Odd, we shouldn't get anything but a version message from
				// a newly connected peer.
				if( Message.get() == NULL ) {
					log() << "[PEER] " << incoming.size() << " bytes left pending" << endl;
					// Leave the loop when no more messages are on it
					break;
				} else {
					log() << "[PEER] Ignoring " << *Message.get() << endl;
				}
			}
		}

		if( VerackReceived && VersionSent ) {
			log() << "[PEER] Handshake complete, setting state to 'Connected'" << endl;
			State = Connected;
		}
	}

	if( State == Connected ) {
//		log() << "[PEER] State: Connected" << endl;
		if( Factory == NULL )
			throw logic_error( "TBitcoinPeer::receive() must have factory in connected mode" );

		try {
			Factory->receive(incoming);
		} catch( exception &e ) {
			log() << "[PEER] Error ";
			TLog::hexify( log(), incoming );
			log() << endl;
			log() << "[PEER] Error parsing message, " << e.what() << endl;
			return;
		}

		auto_ptr<TMessage> Message;

		do {
			Message.reset( nextIncoming() );
			if( Message.get() == NULL )
				break;

			// The network gets to process the packets once we're
			// connected
			Network->process( Message.get() );

		} while( true );
	}
}


// -------------- Class member definitions


// -------------- Function definitions


#ifdef UNITTEST
#include <iostream>
#include <sstream>
#include "unittest.h"
#include <general/logstream.h>
#include "bitcoinnetwork.h"

// -------------- main()

int main( int argc, char *argv[] )
{
	try {
		TBitcoinNetwork Network;
		TBitcoinPeer Peer( NULL, &Network );
		Peer.setState( TBitcoinPeer::Connecting );

		const string *p = UNITTESTSampleMessages;
		while( !p->empty() ) {
			// Fake reception of partial messages
			for( unsigned int i = 0; i < p->size(); i += 1000 ) {
				log() << "[TEST] RX< " << p->size() << " bytes @ " << i << endl;
				Peer.receive( p->size() - i < 200
						? *p
						: p->substr(i,200) );
			}
			p++;

			ostringstream oss;
			TMessage *out = Peer.nextOutgoing();
			if( out != NULL ) {
				out->write( oss );
				log() << "[TEST] TX> " << oss.str().size() << endl;
			}
			delete out;
		}

	} catch( std::exception &e ) {
		log() << e.what() << endl;
		return 255;
	}

	return 0;
}
#endif

