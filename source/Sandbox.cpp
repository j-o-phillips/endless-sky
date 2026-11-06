/* Sandbox.cpp
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

#include "Sandbox.h"

#include "Engine.h"
#include "GameData.h"
#include "Gamerules.h"
#include "Logger.h"
#include "MainPanel.h"
#include "Messages.h"
#include "PilotProfile.h"
#include "PlayerInfo.h"
#include "SandboxScenario.h"
#include "Ship.h"
#include "StartConditions.h"
#include "UI.h"

#include <memory>
#include <vector>

using namespace std;

namespace {
	bool enabled = false;
	const SandboxScenario *active = nullptr;
	bool restartRequested = false;
}



void Sandbox::Enable()
{
	enabled = true;
}



bool Sandbox::IsEnabled()
{
	return enabled;
}



bool Sandbox::IsActive()
{
	return active;
}



void Sandbox::Launch(PlayerInfo &player, UI &gamePanels, const SandboxScenario &scenario)
{
	restartRequested = false;
	string problem = scenario.Validate();
	if(!problem.empty())
	{
		Logger::Log("Cannot launch sandbox \"" + scenario.Name() + "\": " + problem, Logger::Level::WARNING);
		return;
	}

	// Leaving a real pilot for the sandbox: keep their progress, exactly as
	// loading a different pilot would.
	if(!active && player.IsLoaded() && player.GetPlanet() && gamePanels.CanSave())
		player.Save();

	// Pick the gamerules before the current pilot is cleared away.
	Gamerules gamerules = GameData::DefaultGamerules();
	if(!scenario.GamerulesPreset().empty())
		gamerules = *GameData::GamerulesPresets().Find(scenario.GamerulesPreset());
	else if(player.Pilot())
		gamerules = player.Pilot()->GetGamerules();

	// Tear down the old game view. This waits for the engine's worker thread.
	gamePanels.Reset();
	gamePanels.CanSave(false);

	// A throwaway pilot that is not registered with the pilot list and never
	// given a name, so it has no save path and can never be written to disk.
	auto pilot = make_shared<PilotProfile>();
	pilot->New(gamerules);
	player.New(StartConditions(), pilot);
	player.SetSystem(*scenario.GetSystem());
	player.SetPlanet(nullptr);

	const SandboxScenario::ShipSpec &flagshipSpec = scenario.Flagship();
	player.GiftShip(flagshipSpec.model, flagshipSpec.name, "");
	const shared_ptr<Ship> &flagship = player.Ships().back();
	flagship->Recharge();

	vector<shared_ptr<Ship>> npcs;
	for(const SandboxScenario::ShipSpec &spec : scenario.NPCs())
		if(shared_ptr<Ship> ship = spec.Instantiate(*scenario.GetSystem()))
			npcs.push_back(ship);

	active = &scenario;

	auto *panel = new MainPanel(player);
	gamePanels.Push(panel);
	Engine &engine = panel->GetEngine();
	// Place() scatters the player's ships randomly, so set the exact
	// positions afterwards, then add the NPCs.
	engine.Place();
	flagshipSpec.Place(*flagship);
	engine.AddSandboxShips(npcs);
	// Same warm-up as MainPanel::OnCallback() after taking off.
	engine.Go();
	engine.Wait();
	engine.Step(true);
	engine.Go();

	Messages::Add({"Sandbox: " + scenario.Name() + ". Press F5 to restart, Escape to leave.",
		GameData::MessageCategories().Get("normal")});
}



void Sandbox::RequestRestart()
{
	if(active)
		restartRequested = true;
}



void Sandbox::Step(PlayerInfo &player, UI &gamePanels)
{
	if(restartRequested && active)
		Launch(player, gamePanels, *active);
	restartRequested = false;
}



void Sandbox::Exit(PlayerInfo &player, UI &gamePanels)
{
	if(!active)
		return;

	active = nullptr;
	restartRequested = false;
	gamePanels.Reset();
	gamePanels.CanSave(false);
	// Same as at startup: bring back whichever real pilot was played last.
	player.LoadRecent();
}
