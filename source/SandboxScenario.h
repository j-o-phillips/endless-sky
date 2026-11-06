/* SandboxScenario.h
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

#include "Personality.h"
#include "Point.h"

#include <memory>
#include <string>
#include <vector>

class DataNode;
class Government;
class Ship;
class System;



// A developer test scenario, defined in data with a "sandbox" root node. It
// describes a system, the player's flagship, and any NPC ships, each with an
// exact position, so that a test setup can be dropped into instantly.
// Sandbox scenarios are only reachable when the game is run with --sandbox.
class SandboxScenario {
public:
	// One ship placed by the scenario.
	class ShipSpec {
	public:
		void Load(const DataNode &node);
		// Create a fresh, fully crewed and charged copy of this ship in the given
		// system, at its specified position. Returns null if the model is invalid.
		std::shared_ptr<Ship> Instantiate(const System &system) const;
		// Move an existing ship to this spec's position, velocity and facing.
		void Place(Ship &ship) const;

	public:
		const Ship *model = nullptr;
		std::string name;
		Point position;
		Point velocity;
		// Facing in degrees: 0 is "up" on screen, 90 is to the right.
		double facing = 0.;
		const Government *government = nullptr;
		Personality personality;
		bool hasPersonality = false;
	};


public:
	void Load(const DataNode &node);

	const std::string &Name() const;
	const std::string &Description() const;
	// Name of a gamerules preset to use. If empty, the sandbox inherits the
	// gamerules of whichever pilot was loaded when it was launched.
	const std::string &GamerulesPreset() const;
	const System *GetSystem() const;
	const ShipSpec &Flagship() const;
	const std::vector<ShipSpec> &NPCs() const;

	// Check that everything this scenario references is defined. Returns an
	// empty string if so, or a description of the problem if not.
	std::string Validate() const;


private:
	std::string name;
	std::string description;
	std::string gamerulesPreset;
	const System *system = nullptr;
	ShipSpec flagship;
	std::vector<ShipSpec> npcs;
};
