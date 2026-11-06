/* SandboxPanel.h
Copyright (c) 2026 by Jake Phillips

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include "Panel.h"

#include <vector>

class PlayerInfo;
class SandboxScenario;
class UI;



// A plain list of the defined sandbox scenarios, shown from the main menu when
// the game is run with --sandbox. Picking one launches it.
class SandboxPanel : public Panel {
public:
	SandboxPanel(PlayerInfo &player, UI &gamePanels);

	virtual void Draw() override;


protected:
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool Click(int x, int y, MouseButton button, int clicks) override;


private:
	void LaunchSelected();
	// The screen-space top edge of the given row.
	double RowTop(int index) const;


private:
	PlayerInfo &player;
	UI &gamePanels;
	std::vector<const SandboxScenario *> scenarios;
	int selected = 0;
};
