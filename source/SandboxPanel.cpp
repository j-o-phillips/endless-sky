/* SandboxPanel.cpp
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

#include "SandboxPanel.h"

#include "Color.h"
#include "Command.h"
#include "shader/FillShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "GameData.h"
#include "Point.h"
#include "Sandbox.h"
#include "SandboxScenario.h"
#include "UI.h"
#include "text/WrappedText.h"

#include <algorithm>
#include <string>

using namespace std;

namespace {
	const double WIDTH = 520.;
	const double PAD = 20.;
	const double TITLE_HEIGHT = 40.;
	const double ROW_HEIGHT = 24.;
	const double DESCRIPTION_HEIGHT = 90.;
	const double FOOTER_HEIGHT = 30.;
}



SandboxPanel::SandboxPanel(PlayerInfo &player, UI &gamePanels)
	: player(player), gamePanels(gamePanels)
{
	for(const auto &it : GameData::SandboxScenarios())
		scenarios.push_back(&it.second);
}



void SandboxPanel::Draw()
{
	DrawBackdrop();

	const Font &font = FontSet::Get(14);
	const Font &bigFont = FontSet::Get(18);
	const Color &bright = *GameData::Colors().Get("bright");
	const Color &medium = *GameData::Colors().Get("medium");
	const Color &dim = *GameData::Colors().Get("dim");
	const Color &faint = *GameData::Colors().Get("faint");
	const Color &background = *GameData::Colors().Get("panel background");

	const int rows = max<int>(1, scenarios.size());
	const double height = TITLE_HEIGHT + rows * ROW_HEIGHT + DESCRIPTION_HEIGHT + FOOTER_HEIGHT;
	const double top = -height / 2.;
	const double left = -WIDTH / 2.;
	FillShader::Fill(Point(0., 0.), Point(WIDTH, height), background);

	bigFont.Draw("Sandbox scenarios", Point(left + PAD, top + 12.), bright);

	if(scenarios.empty())
		font.Draw("No \"sandbox\" scenarios are defined.", Point(left + PAD, RowTop(0) + 4.), medium);
	for(int i = 0; i < static_cast<int>(scenarios.size()); ++i)
	{
		const double rowTop = RowTop(i);
		if(i == selected)
			FillShader::Fill(Point(0., rowTop + ROW_HEIGHT / 2.), Point(WIDTH - PAD, ROW_HEIGHT), faint);
		string label = (i < 9 ? to_string(i + 1) + ".  " : "     ") + scenarios[i]->Name();
		font.Draw(label, Point(left + PAD, rowTop + 4.), i == selected ? bright : medium);
	}

	const double descriptionTop = RowTop(rows) + 10.;
	if(!scenarios.empty() && !scenarios[selected]->Description().empty())
	{
		WrappedText wrap(font);
		wrap.SetWrapWidth(WIDTH - 2. * PAD);
		wrap.Wrap(scenarios[selected]->Description());
		wrap.Draw(Point(left + PAD, descriptionTop), medium);
	}

	font.Draw("1-9 / Enter: launch    Up / Down: select    Esc: back",
		Point(left + PAD, top + height - FOOTER_HEIGHT + 6.), dim);
}



bool SandboxPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	const int count = scenarios.size();
	if(key == SDLK_ESCAPE || command.Has(Command::MENU))
		GetUI().Pop(this);
	else if(key == SDLK_UP && count)
		selected = (selected + count - 1) % count;
	else if(key == SDLK_DOWN && count)
		selected = (selected + 1) % count;
	else if(key == SDLK_RETURN || key == SDLK_KP_ENTER)
		LaunchSelected();
	else if(key >= '1' && key <= '9' && key - '1' < count)
	{
		selected = key - '1';
		LaunchSelected();
	}
	else
		return false;

	return true;
}



bool SandboxPanel::Click(int x, int y, MouseButton button, int clicks)
{
	if(button != MouseButton::LEFT)
		return false;
	if(x < -WIDTH / 2. || x > WIDTH / 2.)
		return true;

	for(int i = 0; i < static_cast<int>(scenarios.size()); ++i)
		if(y >= RowTop(i) && y < RowTop(i) + ROW_HEIGHT)
		{
			if(selected == i && clicks >= 2)
				LaunchSelected();
			selected = i;
			break;
		}
	return true;
}



void SandboxPanel::LaunchSelected()
{
	if(scenarios.empty())
		return;

	Sandbox::Launch(player, gamePanels, *scenarios[selected]);
	if(Sandbox::IsActive())
		// Close the whole menu stack (including the main menu) and start flying.
		GetUI().PopThrough(GetUI().Root().get());
}



double SandboxPanel::RowTop(int index) const
{
	const int rows = max<int>(1, scenarios.size());
	const double height = TITLE_HEIGHT + rows * ROW_HEIGHT + DESCRIPTION_HEIGHT + FOOTER_HEIGHT;
	return -height / 2. + TITLE_HEIGHT + index * ROW_HEIGHT;
}
