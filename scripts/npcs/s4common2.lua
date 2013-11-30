--[[
Copyright (C) 2008-2013 Vana Development Team

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
--]]
-- Carta

dofile("scripts/lua_functions/jobFunctions.lua");
dofile("scripts/lua_functions/partyQuestHelper.lua");

if isQuestActive(6301) then
	if getPartyId() ~= 0 then
		if getItemAmount(4031472) < 40 then
			if getItemAmount(4000175) > 0 then
				party = getAllPartyPlayerIds();
				if getPartyId() == 0 then
					addText("You don't have a  party. You can challenge with party.");
					sendOK();
				else
					fail = false;
					for i = 1, #party do
						p = party[i];
						if setPlayer(p) then
							if getJobProgression() ~= 2 then
								fail = true;
							end
							revertPlayer();
						end
						if fail then break; end
					end

					if fail then
						addText("You can't enter if anyone in your party hasn't make 4th job advancement.");
						sendOK();
					else
						if not isInstance("herosWill") then
							giveItem(4000175, -1);
							createInstance("herosWill", 5 * 60, true);
							addPartyMembersToInstance();
							moveLocalPartyMembersToMap(923000000);
						else
							addText("Other parties are challenging on quest clear now. Try again later.");
							sendOk();
						end
					end
				end
			else
				addText("Without #b#t4000175##k, you can't enter Cracked Dimension.");
				sendOk();
			end
		else
			addText("If you have 40 #b#t4031472##k, you need no more.");
			sendOk();
		end
	else
		addText("Only party leader can apply to enter. Please get your representative to talk to me.");
		sendOk();
	end
else
	addText("Cracked Dimension? Where did you hear that?");
	sendOk();
end