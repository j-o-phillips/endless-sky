/* Sandbox.h
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

class PlayerInfo;
class SandboxScenario;
class UI;



// Developer sandbox: when the game is started with --sandbox, the main menu
// offers a list of SandboxScenarios. Launching one replaces the loaded pilot
// with a throwaway, unnamed pilot that can never be saved, and drops it
// straight into space. Returning to the main menu ends the sandbox and
// reloads the most recent real pilot from disk.
class Sandbox {
public:
	// Whether the --sandbox command line flag was given.
	static void Enable();
	static bool IsEnabled();

	// Whether a sandbox scenario is currently being played.
	static bool IsActive();

	// Start (or restart) the given scenario. This must be called from outside
	// of the game panels, because it rebuilds them.
	static void Launch(PlayerInfo &player, UI &gamePanels, const SandboxScenario &scenario);
	// Ask for the active scenario to be restarted at the start of the next frame.
	static void RequestRestart();
	// Called once per frame by the main loop, outside of any panel.
	static void Step(PlayerInfo &player, UI &gamePanels);
	// Leave the sandbox, restoring the most recently saved real pilot.
	static void Exit(PlayerInfo &player, UI &gamePanels);
};
