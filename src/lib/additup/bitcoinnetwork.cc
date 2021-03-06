// ----------------------------------------------------------------------------
// Project: additup
/// @file   bitcoinnetwork.cc
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
#include "bitcoinnetwork.h"

// -------------- Includes
// --- C
#include <time.h>
// --- C++
#include <sstream>
// --- Qt
// --- OS
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
// --- Project libs
#include <general/logstream.h>
#include <general/bytearray.h>
#include <general/extraexcept.h>
// --- Project
#include "messages.h"
#include "blockchain.h"
#include "transactions.h"


// -------------- Namespace


// -------------- Module Globals


// -------------- World Globals (need "extern"s in header)


// -------------- Template instantiations


// -------------- Class declarations


// -------------- Class member definitions

//
// Static:	TNetworkParameters :: NULL_REFERENCE_HASH
// Description:
//
const TBitcoinHash TNetworkParameters::NULL_REFERENCE_HASH = 0;

//
// Static:	TNetworkParameters :: NULL_REFERENCE_INDEX
// Description:
//
const unsigned int TNetworkParameters::NULL_REFERENCE_INDEX = static_cast<unsigned int>(-1);

//
// Function:	TNetworkParameters
// Description:
//
TNetworkParameters::TNetworkParameters() :
	ProtocolVersion(0),
	DefaultTCPPort(0),
	Magic(0),
	AddressClass(0)
{
	// Zero for proof of work limit is actually the hardest possible
	// difficulty (if not impossible, as SHA256 won't produce a zero
	// hash).  Therefore we'll default the proof of work limit to the
	// easiest, which is 256 bits of ones.
	ProofOfWorkLimit = (TBitcoinHash(1) << (256)) - 1;

	// Default to something sensible (from official client)
	COINBASE_MATURITY = 100;
	COINBASE_MAXIMUM_SCRIPT_SIZE = 2;
	COINBASE_MAXIMUM_SCRIPT_SIZE = 100;

	MAX_BLOCK_SIZE = 1000000;
	MAX_BLOCK_SIZE_GEN = MAX_BLOCK_SIZE/2;
	MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE/50;

	MINIMUM_TRANSACTION_SIZE = 100;

	BLOCK_TIMESTAMP_WINDOW = 2 * 60 * 60;
	DIFFICULTY_TIMESPAN = 14 * 24 * 60 * 60;
	NEW_BLOCK_PERIOD = 10 * 60;

	// Block reward starts at 50, and is halved every four years.  Only
	// it's not every four years, it's every 210,000 blocks
	//    int64 nSubsidy = 50 * COIN;
	//    nSubsidy >>= (nHeight / 210000);
	INITIAL_MINING_REWARD.setValue(50);
	INFLATION_PERIOD = 210000;

	// From http://en.wikipedia.org/wiki/Series_%28mathematics%29
	//
	// If,
	//
	//   S = SUM(0,infinity, 1/(2^n)) = 1 + 1/2 + 1/4 + 1/8 + ... + 1/2^i + ...
	//   S/2 = 1/2 + 1/4 + 1/8 + 1/16 + ... + 1/2^i + ...
	//
	// Therefore,
	//
	//     S - S/2 = 1
	//   S(1 -1/2) = 1
	//           S = 2
	//
	// The total bitcoins issued can be calculated from this result.
	// Since, the total coins is given by:
	//
	//   T = 50*210000*1 + 50*210000*1/2 + 50*210000*1/4 + ...
	//     = 50*210000*(1 + 1/2 + 1/4 + ...)
	//     = 50*210000*S
	//
	// We know S=2 already,
	//
	//  T = 50 * 210000 * 2
	//    = 21,000,000
	//
	MIN_TX_FEE.setValue(0,50000);
	MIN_MONEY.setValue(0,0);
	MAX_MONEY = INITIAL_MINING_REWARD * INFLATION_PERIOD * 2;

	INV_MAX = 50000;
	GETDATA_MAX = 50000;
	GETBLOCKS_RESPONSES_MAX = 500;
	GETHEADERS_RESPONSES_MAX = 2000;
	ADDR_MAX = 1000;
	ADDR_MIN_TIME = 100000000;
	ADDR_MAX_TIME_OFFSET = 10 * 60;
	ADDR_DEFAULT_TIME_PENALTY = 2 * 60 * 60;

	// Address directory constants
	ASSUME_OFFLINE_AFTER = 24 * 60 * 60;
	OFFLINE_UPDATE_INTERVAL = 24 * 60 * 60;
	ONLINE_UPDATE_INTERVAL = 60 * 60;

//	DifficultyIncreaseSpacing = DIFFICULTY_TIMESPAN / NEW_BLOCK_PERIOD
}

//
// Function:	TNetworkParameters :: convertTargetToDifficulty
// Description:
// Calculate ProofOfWorkLimit / Target as a floating point number.
//
// The official client cheats a fair bit and assumes the two numbers are
// within one uint of each other.  It would be nicer if we could do a
// proper job, but as the official client does this, so will I.
//
double TNetworkParameters::convertTargetToDifficulty( const TBitcoinHash &Target ) const
{
	TBitcoinHash q(Target), r(ProofOfWorkLimit);
	double Difficulty;

//	r.divideWithRemainder(Target, q);
//
//	Difficulty = r;
//
//	// XXX: Fractional part is r/Target

	// Get the highest of the highest bits
	unsigned int hb1, hb2;
	hb1 = q.highestBit();
	hb2 = r.highestBit();
	if( hb2 > hb1 )
		hb1 = hb2;

	// Preserve the top N bits (with N being the storage unit of the big
	// number)
	hb1 -= TBitcoinHash::bitsPerBlock;
	q >>= hb1;
	r >>= hb1;

	// We can now be sure that the two number fill one block only of the
	// big numbers.  We'll let the compiler do the conversion to
	// floating point for us.
	Difficulty = static_cast<double>( r.getBlock(0) )
		/ static_cast<double>( q.getBlock(0) );

	return Difficulty;
}

//
// Function:	TNetworkParameters :: convertDifficultyToTarget
// Description:
//
TBitcoinHash TNetworkParameters::convertDifficultyToTarget( double ) const
{
	// Yuck.
	throw logic_error( "Don't call TNetworkParameters::convertDifficultyToTarget() until I've written it" );
}

//
// Function:	TNetworkParameters :: expectedGHashesPerBlock
// Description:
//
unsigned int TNetworkParameters::expectedGHashesPerBlock( const TBitcoinHash &Target ) const
{
	TBitcoinHash Hashes;
	// To generate a block, the nonce must be selected such that the
	// hash of the block must be less or equal to the current target.
	//
	// There are 2^256 possible hashes, and the current target divides
	// those into two -- above and below it.  Assuming that every hash
	// is equally likely, then the probability of finding an acceptable
	// hash will be
	//
	//    P = target / 2^256
	//
	// Imagine that target was 2^256; the probability would be 100% that
	// any given hash was acceptable -- we would need only 1.  Imagine
	// it was 2^255, half of all hashes would be above and half below;
	// 50% probability -- we would need two hashes.  2^254 would be 25%
	// and we would need 4 hashes.  And so on and so on.  In other
	// words:
	//
	//   N = 2^256 / target
	//
	// Where N is the expected number of hashes performed to find an
	// acceptable block.
	//
	Hashes = (TBitcoinHash(1) << 256);
	Hashes /= Target;

	// NB: Difficulty = MaxTarget / CurrentTarget
	//              N = 2^256 / CurrentTarget
	//              N = 2^256 * Difficulty / MaxTarget

	// NB: We also know that the expected time between blocks is
	// NEW_BLOCK_PERIOD (which we might call SECONDS_PER_BLOCK); we have
	// calculated the HASHES_PER_BLOCK_PERIOD, therefore, the computing
	// power of the network is approximately:
	//
	//   HASHES_PER_SECOND = HASHES_PER_BLOCK_PERIOD / NEW_BLOCK_PERIOD
	//

	// The proof of work limit for the production network is (2^224)-1,
	// which we can use to tell us the sort of scale we are talking
	// about.
	//
	//   2^256 / (2^224-1) = 2^32 (as near as makes no difference)
	//
	// So 4 gigahashes is the minimum expected number of hashes needed
	// to create a block.  The running network will presumably exceed this,
	// and we want space to express that.  Therefore we'll return our
	// answer in gigahashes, which gives plenty of room for expansion.

	Hashes /= 1000000000;

	return Hashes.getBlock(0);
}

// -----------

//
// Function:	TBitcoinNetwork :: TBitcoinNetwork
// Description:
//
TBitcoinNetwork::TBitcoinNetwork() :
	Parameters( NULL ),
	Nonce( 0 ),
	Self( NULL ),
	TransactionPool( NULL ),
	BlockPool( NULL ),
	NetworkTimeOffset( 0 ),
	EventObject( &NULLEventObject )
{
	BlockPool = new TBlockMemoryPool( this );
	TransactionPool = new TMemoryTransactionPool( this );
}

//
// Function:	TBitcoinNetwork :: updateDirectory
// Description:
//
TNodeInfo &TBitcoinNetwork::updateDirectory( const TNodeInfo &Node )
{
	Directory.push_back( Node );
	// Check for duplicates
	// Update timestamps

	// What comes in is not necessarily what goes out.  We are
	// converting a temporary incoming to a permanent entry in our
	// directory.  This allows us to track connection attempts.
	return Directory.back();
}

//
// Function:	TBitcoinNetwork :: process
// Description:
//
void TBitcoinNetwork::process( TMessage *Message )
{
	EventObject->messageReceived( Message );

	if( Message == NULL ) {
		// Spontaneous
	} else if( dynamic_cast<TMessageUnimplemented*>( Message ) != NULL ) {
		// No response needed
		log(TLog::Warning) << "[NETW] Ignoring unknown message type, " << Message->className() << endl;
	} else if( dynamic_cast<TMessage_version*>( Message ) != NULL ) {
		receive_version( reinterpret_cast<TMessage_version*>( Message ) );
	} else if( dynamic_cast<TMessage_inv*>( Message ) != NULL ) {
		receive_inv( reinterpret_cast<TMessage_inv*>( Message ) );
	} else if( dynamic_cast<TMessage_getdata*>( Message ) != NULL ) {
		receive_getdata( reinterpret_cast<TMessage_getdata*>( Message ) );
	} else if( dynamic_cast<TMessage_getblocks*>( Message ) != NULL ) {
		receive_getblocks( reinterpret_cast<TMessage_getblocks*>( Message ) );
	} else if( dynamic_cast<TMessage_getheaders*>( Message ) != NULL ) {
		receive_getheaders( reinterpret_cast<TMessage_getheaders*>( Message ) );
	} else if( dynamic_cast<TMessage_getaddr*>( Message ) != NULL ) {
		receive_getaddr( reinterpret_cast<TMessage_getaddr*>( Message ) );
	} else if( dynamic_cast<TMessage_tx*>( Message ) != NULL ) {
		receive_tx( reinterpret_cast<TMessage_tx*>( Message ) );
	} else if( dynamic_cast<TMessage_block*>( Message ) != NULL ) {
		receive_block( reinterpret_cast<TMessage_block*>( Message ) );
	} else if( dynamic_cast<TMessage_headers*>( Message ) != NULL ) {
		receive_headers( reinterpret_cast<TMessage_headers*>( Message ) );
	} else if( dynamic_cast<TMessage_addr*>( Message ) != NULL ) {
		receive_addr( reinterpret_cast<TMessage_addr*>( Message ) );
	} else if( dynamic_cast<TMessage_reply*>( Message ) != NULL ) {
		receive_reply( reinterpret_cast<TMessage_reply*>( Message ) );
	} else if( dynamic_cast<TMessage_ping*>( Message ) != NULL ) {
		receive_ping( reinterpret_cast<TMessage_ping*>( Message ) );
	} else if( dynamic_cast<TMessage_submitorder*>( Message ) != NULL ) {
		receive_submitorder( reinterpret_cast<TMessage_submitorder*>( Message ) );
	} else if( dynamic_cast<TMessage_checkorder*>( Message ) != NULL ) {
		receive_checkorder( reinterpret_cast<TMessage_checkorder*>( Message ) );
	} else if( dynamic_cast<TMessage_alert*>( Message ) != NULL ) {
		receive_alert( reinterpret_cast<TMessage_alert*>( Message ) );
	}
}

//
// Function:	TBitcoinNetwork :: receive_version
// Description:
//
void TBitcoinNetwork::receive_version( TMessage_version *version )
{
	// We must make this concession to versioning.  Perhaps it would
	// be better to have version-specific TBitcoinNetwork classes,
	// but I'm afraid I can't quite be bothered to do that just for
	// the sake of this if()
	if( version->getVersion() > 20900 ) {
		// Acknowledge every version received, even if the remote
		// chooses to send more than one (which it shouldn't)
		version->getPeer()->queueOutgoing( new TMessage_verack() );
	}

//	if( !Inbound ) {
//		// XXX: Official client does getaddr
//	}

	// XXX: Request block updates since our latest

	// XXX: Pending alerts

}

//
// Function:	TBitcoinNetwork :: receive_verack
// Description:
//
void TBitcoinNetwork::receive_verack( TMessage_verack *verack )
{
}

//
// Function:	TBitcoinNetwork :: receive_inv
// Description:
// The inventory message announces the availability of new transactions
// or new blocks.  It will be received unrequested.
//
void TBitcoinNetwork::receive_inv( TMessage_inv *inv )
{
	if( inv->size() > getNetworkParameters()->INV_MAX )
		return;

	// Announcing a new block
	// RX< inv
	// TX> getdata
	// RX< block
	BlockPool->receiveInventory( inv );
	// Announcing a new transaction
	// RX< inv
	// TX> getdata
	// RX< tx
	TransactionPool->receiveInventory( inv );
}

//
// Function:	TBitcoinNetwork :: receive_getdata
// Description:
// The getdata message is a request that the peer be sent a copy of a
// particular block or a particular transaction.
//
void TBitcoinNetwork::receive_getdata( TMessage_getdata *getdata )
{
	TBitcoinPeer *Peer = getdata->getPeer();

	if( getdata->size() > getNetworkParameters()->GETDATA_MAX )
		return;

	for( unsigned int i = 0; i < getdata->size(); i++ ) {
		// RX< getdata
		TInventoryElement &inv( (*getdata)[i] );
		if( inv.ObjectType == TInventoryElement::MSG_BLOCK ) {
			// TX> block
			BlockPool->queueBlock( Peer, inv.Hash );
			// --- continuation of earlier getblocks
			if( Peer->getContinuationHash() == inv.Hash ) {
				TMessage_inv *inv = new TMessage_inv;
				// From official client:
				// "Bypass PushInventory, this must send even if
				// redundant, and we want it right after the last
				// block so they don't wait for other stuff first."
				// Don't think the above is relevant to us
				TInventoryElement &elem( inv->appendInventory() );
				elem.ObjectType = TInventoryElement::MSG_BLOCK;
				elem.Hash = BlockPool->getBestBranch()->getHash();
				// We have done the continuation
				Peer->setContinuationHash( TBitcoinHash() );
				// Send it along
				Peer->queueOutgoing( inv );
			}
		} else if( inv.ObjectType == TInventoryElement::MSG_TX ) {
			// TX> tx
//			TransactionPool->queueTransaction( Peer, inv.Hash );
		} else if( inv.ObjectType == TInventoryElement::ERROR ) {
			EventObject->inventoryHashError( &inv );
		}
	}
}

//
// Function:	TBitcoinNetwork :: receive_getblocks
// Description:
//
// The remote sends us a list of blocks it has, and we are to send it an
// inventory containing blocks it doesn't have, but we only show it
// blocks in the main chain.
//
// The blocks-it-has-list it sends are most likely the last blockchain
// tips it had received.  It doesn't know which one of them became the
// main chain so we must help it by sending only the main chain.  (I'm
// not sure why this is so; for a peer-to-peer network, it would be
// better for the peer to decide for itself what it thinks the main
// chain is).
//
void TBitcoinNetwork::receive_getblocks( TMessage_getblocks *getblocks )
{
	TBitcoinPeer *Peer = getblocks->getPeer();
	TMessage_inv *inv = new TMessage_inv;
	// RX< getblocks

	for( unsigned int i = 0; i < getblocks->size(); i++ ) {
		const TBitcoinHash &BlockHash = (*getblocks)[i];
		const TBlock *Block = BlockPool->getBlock( BlockHash );

		// If we don't have that block, then we can't supply its
		// children
		if( Block == NULL )
			continue;

		// We only send blocks from what we consider the best chain
		if( !Block->isAncestorOf( BlockPool->getBestBranch() ) )
			continue;

		Peer->setContinuationHash( TBitcoinHash() );

		unsigned int Limit = getNetworkParameters()->GETBLOCKS_RESPONSES_MAX;
		while( true ) {
			Block = Block->getChildOnBranch( BlockPool->getBestBranch() );
			// No more blocks
			if( Block == NULL )
				break;
			// Caller requested a stop
			if( Block->getHash() == getblocks->getStop() )
				break;

			// append this block's hash to the inventory
			TInventoryElement &elem( inv->appendInventory() );
			elem.ObjectType = TInventoryElement::MSG_BLOCK;
			elem.Hash = Block->getHash();

			// If we need to send more than is allowed, then we have
			// to defer our next inv until they have requested the
			// last block we've just offered.
			// This is dreadful.  This makes the bitcoin protocol
			// stateful, which it shouldn't be.  See also getdata
			Limit--;
			if( Limit == 0 ) {
				Peer->setContinuationHash( Block->getHash() );
				break;
			}
		}
	}
	// TX> inv
	if( inv->size() > 0 ) {
		Peer->queueOutgoing( inv );
	} else {
		delete inv;
	}

	// We expect this 'inv' to result in a 'getdata' request from the
	// peer; since it was asking about blocks it doesn't have.
}

//
// Function:	TBitcoinNetwork :: receive_getheaders
// Description:
//
void TBitcoinNetwork::receive_getheaders( TMessage_getheaders *getheaders )
{
	TMessage_headers *headers = new TMessage_headers;
	// RX< getheaders

	for( unsigned int i = 0; i < getheaders->size(); i++ ) {
		const TBitcoinHash &BlockHash = (*getheaders)[i];
		const TBlock *Block = BlockPool->getBlock( BlockHash );

		// If we don't have that block, then we can't supply its
		// children
		if( Block == NULL )
			continue;

		// We only send blocks from what we consider the best chain
		if( !Block->isAncestorOf( BlockPool->getBestBranch() ) )
			continue;

		unsigned int Limit = getNetworkParameters()->GETHEADERS_RESPONSES_MAX;
		while( true ) {
			Block = Block->getChildOnBranch( BlockPool->getBestBranch() );
			// No more blocks
			if( Block == NULL )
				break;
			// Caller requested a stop
			if( Block->getHash() == getheaders->getStop() )
				break;
			// append this block header to the mesage
			Block->writeToHeader( headers->appendBlockHeader() );
			// Limit
			Limit--;
			if( Limit == 0 )
				break;
		}
	}
	// TX> headers
	if( headers->size() > 0 ) {
		getheaders->getPeer()->queueOutgoing( headers );
	} else {
		delete headers;
	}
}

//
// Function:	TBitcoinNetwork :: receive_getaddr
// Description:
//
// "The getaddr message sends a request to a node asking for
// information about known active peers to help with identifying
// potential nodes in the network. The response to receiving
// this message is to transmit an addr message with one or more
// peers from a database of known active peers. The typical
// presumption is that a node is likely to be active if it has
// been sending a message within the last three hours."
//
void TBitcoinNetwork::receive_getaddr( TMessage_getaddr *getaddr )
{
//	Answer = new TMessage_addr();
}

//
// Function:	TBitcoinNetwork :: receive_tx
// Description:
//
void TBitcoinNetwork::receive_tx( TMessage_tx *tx )
{
	try {
		// Pass the message straight to the transaction pool
		TransactionPool->receiveTransaction( tx );
		log() << "[NETW] Transactions in pool " << TransactionPool->size() << endl;
	} catch( exception &e ) {
		log() << "[NETW] Rejecting transaction " << *tx << ", " << e.what() << endl;
	}
}

//
// Function:	TBitcoinNetwork :: receive_block
// Description:
//
void TBitcoinNetwork::receive_block( TMessage_block *block )
{
	// Pass the message straight to the block pool
	try {
		BlockPool->receiveBlock( block );
		log() << "[NETW] Blocks in pool " << BlockPool->size() << endl;
	} catch( exception &e ) {
		log() << "[NETW] Rejecting block " << *block << ", " << e.what() << endl;
	}
}

//
// Function:	TBitcoinNetwork :: receive_headers
// Description:
//
void TBitcoinNetwork::receive_headers( TMessage_headers *headers )
{
	// Pass the message straight to the block pool
	BlockPool->receiveHeaders( headers );
	// No response needed
}

//
// Function:	TBitcoinNetwork :: receive_addr
// Description:
//
void TBitcoinNetwork::receive_addr( TMessage_addr *addr )
{
	addr->updateNetworkDirectory();
	// No response needed
}

//
// Function:	TBitcoinNetwork :: receive_reply
// Description:
//
void TBitcoinNetwork::receive_reply( TMessage_reply *reply )
{
	// No response needed
}

//
// Function:	TBitcoinNetwork :: receive_ping
// Description:
//
void TBitcoinNetwork::receive_ping( TMessage_ping *ping )
{
	// No response needed
}

//
// Function:	TBitcoinNetwork :: receive_submitorder
// Description:
//
// RX< submitorder
// TX> reply
//
void TBitcoinNetwork::receive_submitorder( TMessage_submitorder *submitorder )
{
}

//
// Function:	TBitcoinNetwork :: receive_checkorder
// Description:
//
// RX< checkorder
// TX> reply
//
void TBitcoinNetwork::receive_checkorder( TMessage_checkorder *checkorder )
{
}

//
// Function:	TBitcoinNetwork :: receive_alert
// Description:
//
void TBitcoinNetwork::receive_alert( TMessage_alert *alert )
{
	// No response needed
}

//
// Function:	TBitcoinNetwork :: createMyVersionMessage
// Description:
//
TMessage_version *TBitcoinNetwork::createMyVersionMessage() const
{
	return new TMessage_version_31402;
}

//
// Function:	TBitcoinNetwork :: registerEventObject
// Description:
//
void TBitcoinNetwork::registerEventObject( const TBitcoinEventObject *O )
{
	if( O == NULL ) {
		EventObject = &NULLEventObject;
	} else {
		EventObject = O;
	}
}

//
// Function:	TBitcoinNetwork :: getNetworkTime
// Description:
//
time_t TBitcoinNetwork::getNetworkTime() const
{
	return time(NULL) + NetworkTimeOffset;
}

//
// Function:	TBitcoinNetwork :: connectToAny
// Description:
//
void TBitcoinNetwork::connectToAny()
{
	// Pick a node, then call connect
	connectToNode( Directory.front() );
}

//
// Function:	TBitcoinNetwork :: connectToNode
// Description:
//
void TBitcoinNetwork::connectToNode( const TNodeInfo &n )
{
	// Ensure Node is in our local directory, so that the peer may rely
	// on it existing for its lifetime
	TNodeInfo *Node = &updateDirectory( n );

	log() << "Connect to ";
	Node->printOn( log() );
	log() << endl;

	TBitcoinPeer *Peer;
	Peer = new TBitcoinPeer( Node, this );

	Peer->setState( TBitcoinPeer::Connecting );

	Node->LastConnectAttempt = time(NULL);
}

// -----------

//
// Function:	TBitcoinNetwork_Sockets :: TBitcoinNetwork_Sockets
// Description:
//
TBitcoinNetwork_Sockets::TBitcoinNetwork_Sockets()
{
}

//
// Function:	TBitcoinNetwork_Sockets :: run
// Description:
// It's expected that this function will be the entry point for users of
// this library to fire-and-forget.
//
// We expect to be running in a separate thread, so will not concern
// ourselves with non-blocking I/O.
//
void TBitcoinNetwork_Sockets::run()
{
	fd_set readfds;
	fd_t maxfd;
	map<TBitcoinPeer *, fd_t>::iterator it;
	int ret;
	struct timeval timeout;

	log() << "[NETW] *** TBitcoinNetwork running" << endl;

	log() << "[NETW] " << Directory.size() << " directory entries available" << endl;

	log() << "[NETW] --- TBitcoinNetwork main loop started" << endl;
	while( true ) {
		// Check for low connections
		if( PeerDescriptors.size() < 1 ) {
			log() << "[NETW] Connectivity is low, " << PeerDescriptors.size() << endl;
			connectToAny();
		}

		// Prepare the monitor list, and send pending packets while
		// we're at it
		FD_ZERO( &readfds );
		maxfd = 0;
		for( it = PeerDescriptors.begin(); it != PeerDescriptors.end(); it++ ) {
			FD_SET( it->second, &readfds );
			if( maxfd < it->second )
				maxfd = it->second;

			sendTo( it->first );
		}
		// Prepare timeout
		timeout.tv_sec = 60;
		timeout.tv_usec = 0;

		// Select
//		log() << "[NETW] Waiting for data" << endl;
		ret = select( maxfd + 1, &readfds, NULL, NULL, &timeout );
		if( ret == 0 ) {
			log() << "[NETW] No data received for 60s" << endl;
			continue;
		} else if( ret < 0 ) {
			throw libc_error( "select()" );
		}

		// Find the peers that match the activated descriptors
		for( it = PeerDescriptors.begin(); it != PeerDescriptors.end(); it++ ) {
			if( FD_ISSET( it->second, &readfds ) )
				receiveFrom( it->first );
		}
	}
	log() << "[NETW] --- TBitcoinNetwork main loop finished" << endl;
}

//
// Function:	TBitcoinNetwork_Sockets :: receiveFrom
// Description:
//
void TBitcoinNetwork_Sockets::receiveFrom( TBitcoinPeer *Peer )
{
	fd_t fd = PeerDescriptors[Peer];
	int n;
	TByteArray ba(1024);

	// Perform the read
	n = read( fd, ba, ba.size() );
	if( n == 0 ) {
		// end of file
		disconnect( Peer );
		return;
	} else if( n < 0 ) {
		throw libc_error( "read()" );
	}
	// How many bytes were actually read?
	ba.resize(n);

//	log() << "[NETW] RX< ";
//	TLog::hexify( log(), ba );
//	log() << endl;

	// let the peer deal with what it received
	Peer->receive( ba );
}

//
// Function:	TBitcoinNetwork_Sockets :: sendTo
// Description:
//
void TBitcoinNetwork_Sockets::sendTo( TBitcoinPeer *Peer )
{
	fd_t fd = PeerDescriptors[Peer];
	ostringstream oss;
	TMessage *out;

	while( (out = Peer->nextOutgoing()) ) {
		oss.str("");
		out->write( oss );
//		log() << "[NETW] TX> " << *out << " -> ";
//		TLog::hexify( log(), oss.str() );
//		log() << " (" << oss.str().size() << ")";
//		log() << endl;
		write( fd, oss.str().data(), oss.str().size() );
	}
}

//
// Function:	TBitcoinNetwork_Sockets :: connectToNode
// Description:
//
void TBitcoinNetwork_Sockets::connectToNode( const TNodeInfo &Node )
{
	fd_t fd;
	sockaddr_in SAI;
	sockaddr &SA(reinterpret_cast<sockaddr &>(SAI));
	int ret;

	// Configure the generating fd
	fd = socket( PF_INET, SOCK_STREAM, 0 );
	if( fd < 0 )
		throw socket_error( "socket()" );

	// XXX: iterate through known networks

	// Connect address
	Node.toSockAddr( SA );
	if( SAI.sin_port == 0 )
		SAI.sin_port = htons( 8333 );

	// Connect
	log() << "[NETW] Attempting connection to ";
	Node.printOn( log() );
	log() << endl;
	ret = connect( fd, &SA, sizeof( SAI ) );
	if( ret < 0 )
		throw socket_error( "connect()" );
	log() << "[NETW] Successfully connected, creating TBitcoinPeer object to manage descriptor " << fd << endl;

	// Create a peer to match
	TBitcoinPeer *Peer = new TBitcoinPeer( &Node, this );
	PeerDescriptors[Peer] = fd;
	Peer->setState( TBitcoinPeer::Connecting );

	// Prod the remote to begin
	Peer->receive( TByteArray() );
}

//
// Function:	TBitcoinNetwork_Sockets :: disconnect
// Description:
//
void TBitcoinNetwork_Sockets::disconnect( TBitcoinPeer *Peer )
{
	fd_t fd = PeerDescriptors[Peer];
	int ret;

	ret = shutdown( fd, SHUT_RDWR );
	if( ret < 0 )
		throw socket_error( "shutdown()" );
	ret = close( fd );
	if( ret < 0 )
		throw libc_error( "close()" );

	PeerDescriptors.erase( Peer );
	delete Peer;

	return;
}


// -------------- Function definitions


#ifdef UNITTEST
#include <iostream>
#include <sstream>
#include <general/logstream.h>
#include "constants.h"

//
// Class:	TBitcoinEventAnnounce
// Description:
//
class TBitcoinEventAnnounce : public TBitcoinEventObject
{
  public:
	void messageReceived( const TMessage *Message ) const {
		if( Message != NULL ) {
			log() << "[NETW] RX< " << *Message << endl;
		}
	}
	void inventoryHashError( const TInventoryElement *inv ) const {
		log() << "[NETW] Remote requested ERROR hash " << inv->Hash
			<< ", which I don't know how to handle" << endl;
	}
};

// -------------- main()

int main( int argc, char *argv[] )
{
	try {
		log() << "--- Create network" << endl;
		TBitcoinNetwork_Sockets Network;
		TBitcoinEventAnnounce EventHandler;

		Network.registerEventObject( &EventHandler );

		TNodeInfo localhost( TNodeInfo::fromDottedQuad(127.0.0.1) );
		Network.updateDirectory( localhost );

//		const TOfficialSeedNode *pSeed = SEED_NODES;
//		while( *pSeed ) {
//			Network.updateDirectory( *pSeed );
//			pSeed++;
//		}

		Network.run();

	} catch( std::exception &e ) {
		log() << e.what() << endl;
		return 255;
	}

	return 0;
}
#endif

