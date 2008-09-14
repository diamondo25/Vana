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
#ifndef WORLDSERVER_H
#define WORLDSERVER_H

#include "AbstractServer.h"
#include "ConfigFile.h"
#include "LoginServerConnectPlayer.h"
#include "Types.h"
#include "WorldServerAcceptPlayer.h"
#include <string>

using std::string;

// WorldServer main application class, implemented as singleton
class WorldServer : public AbstractServer {
public:
	static WorldServer * Instance() {
		if (singleton == 0)
			singleton = new WorldServer;
		return singleton;
	}
	void loadData();
	void loadConfig();
	void listen();

	void setWorldId(int8_t id) { worldId = id; }
	void setMaxMultiLevel(uint8_t level) { maxMultiLevel = level; }
	void setInterPort(uint16_t port) { inter_port = port; }
	void setMaxChannels(int32_t maxchan) { maxChannels = maxchan; }
	void setExprate(int32_t exprate) { this->exprate = exprate; }
	void setQuestExprate(int32_t questexprate) { this->questexprate = questexprate; }
	void setMesorate(int32_t mesorate) { this->mesorate = mesorate; }
	void setDroprate(int32_t droprate) { this->droprate = droprate; }
	void setScrollingHeader(const string &message);

	int8_t getWorldId() const { return worldId; }
	uint8_t getMaxMultiLevel() const { return maxMultiLevel; }
	uint16_t getInterPort() const { return inter_port; }
	int32_t getMaxChannels() const { return maxChannels; }
	int32_t getExprate() const { return exprate; }
	int32_t getQuestExprate() const { return questexprate; }
	int32_t getMesorate() const { return mesorate; }
	int32_t getDroprate() const { return droprate; }
	LoginServerConnectPlayer * getLoginPlayer() const { return loginPlayer; }
	string getScrollingHeader() { return scrollingHeader.c_str(); }

private:
	WorldServer() { scrollingHeader = ""; };
	WorldServer(const WorldServer&);
	WorldServer& operator=(const WorldServer&);
	static WorldServer *singleton;

	int8_t worldId;
	uint8_t maxMultiLevel;
	uint16_t login_inter_port;
	uint16_t inter_port;
	int32_t exprate;
	int32_t questexprate;
	int32_t mesorate;
	int32_t droprate;
	int32_t maxChannels;
	string login_ip;
	string external_ip;
	string scrollingHeader;
	LoginServerConnectPlayer *loginPlayer;
};

#endif
