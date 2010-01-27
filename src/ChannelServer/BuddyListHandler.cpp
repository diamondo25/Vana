/*
Copyright (C) 2008-2010 Vana Development Team

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
#include "BuddyListHandler.h"
#include "BuddyListPacket.h"
#include "Player.h"
#include "PacketReader.h"
#include <string>

using std::string;

namespace BuddyOpcodes {
	enum Opcodes {
		Add = 0x01,
		Remove = 0x03
	};
}

void BuddyListHandler::handleBuddyList(Player *player, PacketReader &packet) {
	int8_t type = packet.get<int8_t>();

	switch (type) {
		case BuddyOpcodes::Add: {
			string name = packet.getString();

			uint8_t error = player->getBuddyList()->add(name);

			if (error) {
				BuddyListPacket::error(player, error);
			}
			break;
		}
		case BuddyOpcodes::Remove: {
			int32_t charid = packet.get<int32_t>();
			player->getBuddyList()->remove(charid);
			break;
		}
	}
}
