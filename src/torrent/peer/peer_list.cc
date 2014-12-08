// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#define __STDC_FORMAT_MACROS

#include <algorithm>
#include <functional>
#include <rak/functional.h>
#include <rak/socket_address.h>

#include "download/available_list.h"
#include "torrent/peer/client_list.h"
#include "torrent/utils/log.h"

#include "download_info.h"
#include "exceptions.h"
#include "globals.h"
#include "manager.h"
#include "peer_info.h"
#include "peer_list.h"

#include <fstream>
#include <sstream>


#define LT_LOG_EVENTS(log_fmt, ...)                                     \
  lt_log_print_info(LOG_PEER_LIST_EVENTS, m_info, "peer_list", log_fmt, __VA_ARGS__);
#define LT_LOG_SA_FMT "'%s:%" PRIu16 "'"

namespace torrent {

ipv4_table PeerList::m_ipv4_table;

bool
socket_address_less(const sockaddr* s1, const sockaddr* s2) {
  const rak::socket_address* sa1 = rak::socket_address::cast_from(s1);
  const rak::socket_address* sa2 = rak::socket_address::cast_from(s2);

  if (sa1->family() != sa2->family())
    return sa1->family() < sa2->family();

  else if (sa1->family() == rak::socket_address::af_inet)
    // Sort by hardware byte order to ensure proper ordering for
    // humans.
    return sa1->sa_inet()->address_h() < sa2->sa_inet()->address_h();

  else
    // When we implement INET6 handling, embed the ipv4 address in
    // the ipv6 address.
    throw internal_error("socket_address_key(...) tried to compare an invalid family type.");
}

///////////////////
bool
socket_address_same(const sockaddr* s1, const sockaddr* s2) {
	const rak::socket_address* sa1 = rak::socket_address::cast_from(s1);
	const rak::socket_address* sa2 = rak::socket_address::cast_from(s2);

	return sa1->address_str() == sa2->address_str();
}
///////////////////

inline bool
socket_address_key::is_comparable(const sockaddr* sa) {
  return rak::socket_address::cast_from(sa)->family() == rak::socket_address::af_inet;
}

struct peer_list_equal_port : public std::binary_function<PeerList::reference, uint16_t, bool> {
  bool operator () (PeerList::reference p, uint16_t port) {
    return rak::socket_address::cast_from(p.second->socket_address())->port() == port;
  }
};

//
// PeerList:
//

PeerList::PeerList() :
  m_available_list(new AvailableList) {
}

PeerList::~PeerList() {
  LT_LOG_EVENTS("deleting list total:%" PRIuPTR " available:%" PRIuPTR,
                size(), m_available_list->size());

  std::for_each(begin(), end(), rak::on(rak::mem_ref(&value_type::second), rak::call_delete<PeerInfo>()));
  base_type::clear();

  m_info = NULL;
  delete m_available_list;
}

void
PeerList::set_info(DownloadInfo* info) {
  m_info = info;

  LT_LOG_EVENTS("creating list", 0);
}

PeerInfo*
PeerList::insert_address(const sockaddr* sa, int flags) {
  if (!socket_address_key::is_comparable(sa)) {
    LT_LOG_EVENTS("address not comparable", 0);
    return NULL;
  }

  const rak::socket_address* address = rak::socket_address::cast_from(sa);

  range_type range = base_type::equal_range(sa);

  // Do some special handling if we got a new port number but the
  // address was present.
  //
  // What we do depends on the flags, but for now just allow one
  // PeerInfo per address key and do nothing.
  if (range.first != range.second) {
    LT_LOG_EVENTS("address already exists " LT_LOG_SA_FMT,
                  address->address_str().c_str(), address->port());
    return NULL;
  }

  PeerInfo* peerInfo = new PeerInfo(sa);
  peerInfo->set_listen_port(address->port());
  peerInfo->set_flags(m_ipv4_table.at(address->sa_inet()->address_h()) & PeerInfo::mask_ip_table);
  
  manager->client_list()->retrieve_unknown(&peerInfo->mutable_client_info());

  base_type::insert(range.second, value_type(socket_address_key(peerInfo->socket_address()), peerInfo));

  if ((flags & address_available) && peerInfo->listen_port() != 0) {
    m_available_list->push_back(address);
    LT_LOG_EVENTS("added available address " LT_LOG_SA_FMT,
                  address->address_str().c_str(), address->port());
  } else {
    LT_LOG_EVENTS("added unavailable address " LT_LOG_SA_FMT,
                  address->address_str().c_str(), address->port());
  }

  return peerInfo;
}

//////////////////////////
void
PeerList::updateBatmanAdv_value() {

	std::multimap<int,std::string> temp;
	const std::string* addr;
	rak::socket_address* socketAddress = new rak::socket_address();
	std::ifstream infile;
	char* homeVar = getenv("HOME");
	std::string fullpath;

	fullpath.append(homeVar);
	fullpath.append("/scripts/wibed_batctl.sh");

	infile.open(fullpath.c_str());
	if(infile)
		system(fullpath.c_str());
	infile.close();

	fullpath = homeVar;
	fullpath.append("/scripts/PQuality");

	lt_log_print(LOG_INFO, "filepath: %s", fullpath.c_str());

	infile.open(fullpath.c_str());

	std::string line;
	while (std::getline(infile, line))
	{
	    std::istringstream iss(line);
	    std::string IP;
	    uint16_t Port;
	    int PathQuality;
	    if (!(iss >> IP >> Port >> PathQuality)) { break; }

	    temp.insert(std::pair<int,std::string>(255-PathQuality,IP));
	}

	infile.close();

	batmanValue_List.clear();

	for(std::map<int,std::string>::iterator itt = temp.begin(); itt != temp.end(); itt++) {
		addr = new std::string(itt->second);
		socketAddress = new rak::socket_address();
		socketAddress->set_address_str(*addr);
		batmanValue_List.insert(std::pair<int,rak::socket_address*>(itt->first, socketAddress));
	}


	for(std::multimap<int,rak::socket_address*>::iterator it = batmanValue_List.begin(); it != batmanValue_List.end(); it++)
	{
		lt_log_print(LOG_INFO, "batValue -- %s with value of %d", (*it).second->address_str().c_str(), (*it).first);
	}

}

uint32_t
PeerList::cull_byBatmanAdv(int maxPreferred)
{
	lt_log_print(LOG_INFO, "PeerList::cull_byBatmanAdv, maxPreferred %d", maxPreferred);

	//if current peer list is small then we don't need to do any preference
	if(base_type::size() <= maxPreferred)
		return 0;


	//block all peers
	for (iterator itr = base_type::begin(); itr != base_type::end(); itr++) {
		itr->second->unset_flags(PeerInfo::flag_preferred);
	}


	updateBatmanAdv_value();

	batman_type::iterator it = batmanValue_List.begin();

	int count = 0;
	while(count < maxPreferred)
	{
		base_type::const_iterator pit = begin();
		while(pit != end())
		{
			if(pit->first == it->second->c_sockaddr())
			{
				pit->second->set_flags(PeerInfo::flag_preferred);
				lt_log_print(LOG_INFO, "cull_byBatmanAdv: %s flag is set to preferred", it->second->address_str().c_str());
				break;
			}
			pit++;
		}

		if(pit == end())
			lt_log_print(LOG_INFO, "cull_byBatmanAdv: %s is missing", it->second->address_str().c_str());

		it++;
		count++;
	}

	return 0;
}

///////////////////////////


inline bool
socket_address_less_rak(const rak::socket_address& s1, const rak::socket_address& s2) {
  return socket_address_less(s1.c_sockaddr(), s2.c_sockaddr());
}

uint32_t
PeerList::insert_available(const void* al) {
  const AddressList* addressList = static_cast<const AddressList*>(al);

  uint32_t inserted = 0;
  uint32_t invalid = 0;
  uint32_t unneeded = 0;
  uint32_t updated = 0;

  if (m_available_list->size() + addressList->size() > m_available_list->capacity())
    m_available_list->reserve(m_available_list->size() + addressList->size() + 128);

  // Optimize this so that we don't traverse the tree for every
  // insert, since we know 'al' is sorted.

  AddressList::const_iterator itr   = addressList->begin();
  AddressList::const_iterator last  = addressList->end();
  AvailableList::const_iterator availItr  = m_available_list->begin();
  AvailableList::const_iterator availLast = m_available_list->end();

  for (; itr != last; itr++) {
    if (!socket_address_key::is_comparable(itr->c_sockaddr()) || itr->port() == 0) {
      invalid++;
      continue;
    }

    availItr = std::find_if(availItr, availLast, rak::bind2nd(std::ptr_fun(&socket_address_less_rak), *itr));

    if (availItr != availLast && !socket_address_less(availItr->c_sockaddr(), itr->c_sockaddr())) {
      // The address is already in m_available_list, so don't bother
      // going further.
      unneeded++;
      continue;
    }

    // Check if the peerinfo exists, if it does, check if we would
    // ever want to connect. Just update the timer for the last
    // availability notice if the peer isn't really ideal, but might
    // be used in an emergency.
    range_type range = base_type::equal_range(itr->c_sockaddr());

    if (range.first != range.second) {
      // Add some logic here to select the best PeerInfo, but for now
      // just assume the first one is the only one that exists.
      PeerInfo* peerInfo = range.first->second;

      if (peerInfo->listen_port() == 0)
        peerInfo->set_port(itr->port());

      if (peerInfo->connection() != NULL ||
          peerInfo->last_handshake() + 600 > (uint32_t)cachedTime.seconds()) {
        updated++;
        continue;
      }

      // If the peer has sent us bad chunks or we just connected or
      // tried to do so a few minutes ago, only update its
      // availability timer.
    }

    // Should we perhaps add to available list even though we don't
    // want the peer, just to ensure we don't need to search for the
    // PeerInfo every time it gets reported. Though I'd assume it
    // won't happen often enough to be worth it.

    inserted++;
    m_available_list->push_back(&*itr);
  }

  LT_LOG_EVENTS("inserted peers"
                " inserted:%" PRIu32 " invalid:%" PRIu32
                " unneeded:%" PRIu32 " updated:%" PRIu32
                " total:%" PRIuPTR " available:%" PRIuPTR,
                inserted, invalid, unneeded, updated,
                size(), m_available_list->size());

  return inserted;
}

uint32_t
PeerList::available_list_size() const {
  return m_available_list->size();
}

PeerInfo*
PeerList::connected(const sockaddr* sa, int flags) {
  const rak::socket_address* address = rak::socket_address::cast_from(sa);

  if (!socket_address_key::is_comparable(sa))
    return NULL;

  int filter_value = m_ipv4_table.at(address->sa_inet()->address_h());

  // We should also remove any PeerInfo objects already for this
  // address.
  if ((filter_value & PeerInfo::flag_unwanted))
    return NULL;

  PeerInfo* peerInfo;
  range_type range = base_type::equal_range(sa);

  if (range.first == range.second) {
    // Create a new entry.
    peerInfo = new PeerInfo(sa);
    peerInfo->set_flags(filter_value & PeerInfo::mask_ip_table);

    base_type::insert(range.second, value_type(socket_address_key(peerInfo->socket_address()), peerInfo));

  } else if (!range.first->second->is_connected()) {
    // Use an old entry.
    peerInfo = range.first->second;
    peerInfo->set_port(address->port());

  } else {
    // Make sure we don't end up throwing away the port the host is
    // actually listening on, when there may be several simultaneous
    // connection attempts to/from different ports.
    //
    // This also ensure we can connect to peers running on the same
    // host as the tracker.
    if (flags & connect_keep_handshakes &&
        range.first->second->is_handshake() &&
        rak::socket_address::cast_from(range.first->second->socket_address())->port() != address->port())
      m_available_list->buffer()->push_back(*address);

    return NULL;
  }

  if (flags & connect_filter_recent &&
      peerInfo->last_handshake() + 600 > (uint32_t)cachedTime.seconds())
    return NULL;

  if (!(flags & connect_incoming))
    peerInfo->set_listen_port(address->port());

  if (flags & connect_incoming)
    peerInfo->set_flags(PeerInfo::flag_incoming);
  else
    peerInfo->unset_flags(PeerInfo::flag_incoming);

  peerInfo->set_flags(PeerInfo::flag_connected);
  peerInfo->set_last_handshake(cachedTime.seconds());

  return peerInfo;
}

// Make sure we properly clear port when disconnecting.

void
PeerList::disconnected(PeerInfo* p, int flags) {
  range_type range = base_type::equal_range(p->socket_address());
  
  iterator itr = std::find_if(range.first, range.second, rak::equal(p, rak::mem_ref(&value_type::second)));

  if (itr == range.second) {
    if (std::find_if(base_type::begin(), base_type::end(), rak::equal(p, rak::mem_ref(&value_type::second))) == base_type::end())
      throw internal_error("PeerList::disconnected(...) itr == range.second, doesn't exist.");
    else
      throw internal_error("PeerList::disconnected(...) itr == range.second, not in the range.");
  }
  
  disconnected(itr, flags);
}

PeerList::iterator
PeerList::disconnected(iterator itr, int flags) {
  if (itr == base_type::end())
    throw internal_error("PeerList::disconnected(...) itr == end().");

  if (!itr->second->is_connected())
    throw internal_error("PeerList::disconnected(...) !itr->is_connected().");

  if (itr->second->transfer_counter() != 0) {
    // Currently we only log these as it only affects the culling of
    // peers.
    LT_LOG_EVENTS("disconnected with non-zero transfer counter (%" PRIu32 ") for peer %40s",
                  itr->second->transfer_counter(), itr->second->id_hex());
  }

  itr->second->unset_flags(PeerInfo::flag_connected);

  // Replace the socket address port with the listening port so that
  // future outgoing connections will connect to the right port.
  itr->second->set_port(0);

  if (flags & disconnect_set_time)
    itr->second->set_last_connection(cachedTime.seconds());

  if (flags & disconnect_available && itr->second->listen_port() != 0)
    m_available_list->push_back(rak::socket_address::cast_from(itr->second->socket_address()));

  // Do magic to get rid of unneeded entries.
  return ++itr;
}

uint32_t
PeerList::clear_Peerlist() {

	uint32_t counter = 0;

	for (iterator itr = base_type::begin(); itr != base_type::end(); ) {
	    if (itr->second->is_connected()) {
	      itr++;
	      continue;
	    }

	    iterator tmp = itr++;
	    PeerInfo* peerInfo = tmp->second;

	    base_type::erase(tmp);
	    delete peerInfo;
	    counter++;
	}

	return counter;
}

uint32_t
PeerList::cull_peers(int flags) {
  uint32_t counter = 0;
  uint32_t timer;

  ///////////////
   lt_log_print(LOG_INFO, "PeerList::cull_peers");
  //////////////

  if (flags & cull_old)
    timer = cachedTime.seconds() - 24 * 60 * 60;
  else
    timer = 0;

  for (iterator itr = base_type::begin(); itr != base_type::end(); ) {
    if (itr->second->is_connected() ||
        itr->second->transfer_counter() != 0 || // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        itr->second->last_connection() >= timer ||
        (flags & cull_keep_interesting && 
         (itr->second->failed_counter() != 0 || itr->second->is_blocked()))) {
      itr++;
      continue;
    }

    // ##################### TODO: LOG CULLING OF PEERS ######################
    //   *** AND STATS OF DISCONNECTING PEERS (the peer info...)...

    // The key is a pointer to a member in the value, although the key
    // shouldn't actually be used in erase (I think), just ot be safe
    // we delete it after erase.
    iterator tmp = itr++;
    PeerInfo* peerInfo = tmp->second;

    ///////////////
    //lt_log_print(LOG_INFO, "PeerList::cull_peers, auto remove this node: %s", rak::socket_address::cast_from(peerInfo->socket_address())->address_str().c_str());
    //////////////
    base_type::erase(tmp);
    delete peerInfo;

    counter++;
  }

  return counter;
}

}
