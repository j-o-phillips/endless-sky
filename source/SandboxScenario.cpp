/* SandboxScenario.cpp
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

#include "SandboxScenario.h"

#include "Angle.h"
#include "DataNode.h"
#include "GameData.h"
#include "Gamerules.h"
#include "Government.h"
#include "Ship.h"
#include "System.h"

using namespace std;



void SandboxScenario::ShipSpec::Load(const DataNode &node)
{
	if(node.Size() >= 2)
		model = GameData::Ships().Get(node.Token(1));
	else
		node.PrintTrace("Missing ship model name:");

	for(const DataNode &child : node)
	{
		const string &key = child.Token(0);
		bool hasValue = child.Size() >= 2;
		if(key == "name" && hasValue)
			name = child.Token(1);
		else if(key == "position" && child.Size() >= 3)
			position = Point(child.Value(1), child.Value(2));
		else if(key == "velocity" && child.Size() >= 3)
			velocity = Point(child.Value(1), child.Value(2));
		else if(key == "facing" && hasValue)
			facing = child.Value(1);
		else if(key == "government" && hasValue)
			government = GameData::Governments().Get(child.Token(1));
		else if(key == "personality")
		{
			personality.Load(child);
			hasPersonality = true;
		}
		else
			child.PrintTrace("Skipping unrecognized sandbox ship attribute:");
	}
}



shared_ptr<Ship> SandboxScenario::ShipSpec::Instantiate(const System &system) const
{
	if(!model || !model->IsValid())
		return nullptr;

	auto ship = make_shared<Ship>(*model);
	ship->SetGivenName(name.empty() ? model->DisplayModelName() : name);
	ship->SetSystem(&system);
	ship->SetPlanet(nullptr);
	if(government)
		ship->SetGovernment(government);
	if(hasPersonality)
		ship->SetPersonality(personality);
	ship->Recharge();
	Place(*ship);
	return ship;
}



void SandboxScenario::ShipSpec::Place(Ship &ship) const
{
	ship.Place(position, velocity, Angle(facing), false);
}



void SandboxScenario::Load(const DataNode &node)
{
	if(node.Size() >= 2)
		name = node.Token(1);

	for(const DataNode &child : node)
	{
		const string &key = child.Token(0);
		bool hasValue = child.Size() >= 2;
		if(key == "description" && hasValue)
			description = child.Token(1);
		else if(key == "gamerules" && hasValue)
			gamerulesPreset = child.Token(1);
		else if(key == "system" && hasValue)
			system = GameData::Systems().Get(child.Token(1));
		else if(key == "flagship")
			flagship.Load(child);
		else if(key == "npc")
		{
			npcs.emplace_back();
			npcs.back().Load(child);
		}
		else
			child.PrintTrace("Skipping unrecognized sandbox attribute:");
	}
}



const string &SandboxScenario::Name() const
{
	return name;
}



const string &SandboxScenario::Description() const
{
	return description;
}



const string &SandboxScenario::GamerulesPreset() const
{
	return gamerulesPreset;
}



const System *SandboxScenario::GetSystem() const
{
	return system;
}



const SandboxScenario::ShipSpec &SandboxScenario::Flagship() const
{
	return flagship;
}



const vector<SandboxScenario::ShipSpec> &SandboxScenario::NPCs() const
{
	return npcs;
}



string SandboxScenario::Validate() const
{
	if(!system || !system->IsValid())
		return "its system is missing or undefined.";
	if(!flagship.model || !flagship.model->IsValid())
		return "its flagship model is missing or undefined.";
	for(const ShipSpec &npc : npcs)
	{
		if(!npc.model || !npc.model->IsValid())
			return "one of its NPC ship models is missing or undefined.";
		else if(!npc.government)
			// Ship models default to the player's government, which would put an
			// unspecified NPC on the player's side.
			return "one of its NPCs has no government.";
	}
	if(!gamerulesPreset.empty() && !GameData::GamerulesPresets().Find(gamerulesPreset))
		return "its gamerules preset \"" + gamerulesPreset + "\" is not defined.";
	return "";
}
