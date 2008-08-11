/*
Copyright (C) 2008 Vana Development Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifndef ABSTRACTPLAYER_H
#define ABSTRACTPLAYER_H

#include "NewTimer.h"
#include <string>
#include <boost/scoped_ptr.hpp>

using std::string;

class PacketHandler;
class ReadPacket;

class AbstractPlayer {
public:
	AbstractPlayer();

	virtual void realHandleRequest(ReadPacket *packet) = 0;
	void handleRequest(ReadPacket *packet);
	void setTimer();
	void ping();

	PacketHandler * getPacketHandler() const { return packetHandler; }
	void setPacketHandler(PacketHandler * ph) { packetHandler = ph; }
	string getIP() const { return ip; }
	void setIP(const string &ip) { this->ip = ip; }
	NewTimer::Container * getTimers() const { return timers.get(); }

	virtual ~AbstractPlayer();
protected:
	PacketHandler *packetHandler;
	string ip;
	bool is_server;
private:
	bool is_pinged;
	boost::scoped_ptr<NewTimer::Container> timers;
};

class AbstractPlayerFactory {
public:
	virtual AbstractPlayer * createPlayer () = 0;
};
#endif
