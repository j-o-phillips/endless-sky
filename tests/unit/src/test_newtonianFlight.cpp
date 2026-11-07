/* test_newtonianFlight.cpp
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

#include "es-test.hpp"

// Include only the tested class's header.
#include "../../../source/ship/NewtonianFlight.h"

// ... and any system includes needed for the test file.
#include <cmath>

namespace { // test namespace

// #region mock data

using namespace NewtonianFlight;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

constexpr double PI = 3.14159265358979323846;
constexpr double TO_RAD = PI / 180.;
constexpr double TO_DEG = 180. / PI;

// A mid-sized ship with the default Newtonian tuning: the built-in retro and
// lateral thrust are 19% of the main engine, and spin-up takes 10 frames.
constexpr double RATED_SPEED = 6.;
constexpr double FORWARD = .3;
constexpr double BASELINE = FORWARD * .19;
constexpr double RATED_TURN = 3.;
constexpr double ANGULAR_ACCEL = RATED_TURN / 10.;
constexpr double SOFT_CAP = 2.5;

Thrusters ShipThrusters(double afterburner = 0.)
{
	Thrusters thrusters;
	thrusters.forward = FORWARD;
	thrusters.afterburner = afterburner;
	thrusters.retro = BASELINE;
	thrusters.lateral = BASELINE;
	return thrusters;
}

Input Thrust(double thrust = 1.)
{
	Input input;
	input.thrust = thrust;
	return input;
}

Braking ShipBraking(double lateral = BASELINE)
{
	Braking braking;
	braking.main = FORWARD;
	braking.retro = BASELINE;
	braking.lateral = lateral;
	braking.ratedTurn = RATED_TURN;
	braking.spinUpFrames = RATED_TURN / ANGULAR_ACCEL;
	return braking;
}

Input Stop()
{
	Input input;
	input.stop = true;
	return input;
}

// Facing in degrees, 0 pointing "up" (0, -1) and positive turning right, as Angle does.
Point Unit(double degrees)
{
	return Point(sin(degrees * TO_RAD), -cos(degrees * TO_RAD));
}

// Degrees to turn right to face the given direction, in [-180, 180].
double DeltaTo(double degrees, const Point &direction)
{
	return remainder(atan2(direction.X(), -direction.Y()) * TO_DEG - degrees, 360.);
}

// One ship flown frame by frame the way Ship::DoNewtonianMovement does, with
// unlimited energy so that every planned burn is delivered.
struct Sim {
	bool assist = true;
	double angle = 0.;
	double angularVelocity = 0.;
	Point velocity;
	// Moved by the new velocity after each frame, as Ship::Move does.
	Point position;
	double rated = RATED_SPEED;
	double ratedTurn = RATED_TURN;
	double angularAccel = ANGULAR_ACCEL;
	Thrusters thrusters = ShipThrusters();

	TranslationPlan Step(const Input &input, double turnInput = 0.)
	{
		// Translation uses the facing from the start of the frame.
		const Point facing = Unit(angle);
		RotationPlan rotation = PlanRotation(assist, turnInput, angularVelocity, ratedTurn, angularAccel, SOFT_CAP);
		angularVelocity += rotation.deltaOmega * rotation.capFactor;
		if(std::fabs(angularVelocity) < 1e-6)
			angularVelocity = 0.;
		angle += angularVelocity;

		TranslationPlan plan = PlanTranslation(assist, velocity, facing, rated, thrusters, input, SOFT_CAP);
		velocity = Apply(velocity, facing, plan.dvForward, plan.dvLateral, plan.softCapFactor);
		if(assist && input.stop && velocity.Length() < 1e-3)
			velocity = Point();
		position += velocity;
		return plan;
	}
};

// The angle within which AI::TurnToward (default precision 0.9999) counts a ship
// as facing its target.
const double ALIGNED_DEGREES = std::acos(std::sqrt(.9999)) * TO_DEG;

// The braking a Sim has, as Ship::NewtonianBraking reports it.
Braking SimBraking(const Sim &sim)
{
	Braking braking;
	braking.main = sim.thrusters.forward;
	braking.retro = sim.thrusters.retro;
	braking.lateral = sim.thrusters.lateral;
	braking.ratedTurn = sim.ratedTurn;
	braking.spinUpFrames = sim.ratedTurn / sim.angularAccel;
	return braking;
}

// Fly STOP the way AI::MovePlayer does: decide once whether to flip and burn,
// then turn toward retrograde every frame if so. Returns the frames it took.
int FlyStop(Sim &sim, int maxFrames = 5000)
{
	const bool flip = ShouldFlipAndBurn(sim.velocity, Unit(sim.angle), sim.angularVelocity, SimBraking(sim));
	int frames = 0;
	while(sim.velocity && frames < maxFrames)
	{
		double turn = 0.;
		if(flip)
		{
			double delta = DeltaTo(sim.angle, -sim.velocity);
			turn = TurnToward(true, delta, sim.angularVelocity, sim.ratedTurn, sim.angularAccel,
				std::fabs(delta) < ALIGNED_DEGREES);
		}
		sim.Step(Stop(), turn);
		++frames;
	}
	return frames;
}

// #endregion mock data



// #region unit tests
SCENARIO( "The soft cap above the rated speed", "[NewtonianFlight][softCap]" ) {
	GIVEN( "a rated speed and the default soft cap" ) {
		THEN( "pushes at or below the rated speed are fully effective" ) {
			CHECK( SoftCapFactor(0., RATED_SPEED, SOFT_CAP) == 1. );
			CHECK( SoftCapFactor(RATED_SPEED, RATED_SPEED, SOFT_CAP) == 1. );
		}
		THEN( "a push at the soft cap is 10% effective" ) {
			CHECK_THAT( SoftCapFactor(RATED_SPEED * SOFT_CAP, RATED_SPEED, SOFT_CAP),
				WithinAbs(SOFT_CAP_RESIDUAL, 1e-12) );
		}
		THEN( "the effectiveness falls steadily but never reaches zero" ) {
			double last = 1.;
			for(double speed = RATED_SPEED + .1; speed < RATED_SPEED * 10.; speed += .1)
			{
				double factor = SoftCapFactor(speed, RATED_SPEED, SOFT_CAP);
				CHECK( factor < last );
				CHECK( factor > 0. );
				last = factor;
			}
		}
	}
	GIVEN( "no rated speed or no soft cap" ) {
		THEN( "there is no cap" ) {
			CHECK( SoftCapFactor(10., 0., SOFT_CAP) == 1. );
			CHECK( SoftCapFactor(10., RATED_SPEED, 1.) == 1. );
		}
	}
}

SCENARIO( "Coasting down with no thrust input", "[NewtonianFlight][coast]" ) {
	Sim sim;
	// Sliding sideways and forward at once, so both manoeuvring groups are involved.
	const Point start = Unit(30.) * RATED_SPEED;
	sim.velocity = start;
	GIVEN( "flight assist is on" ) {
		THEN( "the computer brakes on the manoeuvring thrusters, at their pace" ) {
			int frames = 0;
			while(sim.velocity.Length() > 1e-9 && frames < 1000)
			{
				double before = sim.velocity.Length();
				TranslationPlan plan = sim.Step(Input());
				++frames;
				CHECK_FALSE( plan.braking );
				// Never faster than the weaker group, and never reversing.
				CHECK( before - sim.velocity.Length() <= BASELINE + 1e-9 );
				CHECK( sim.velocity.Dot(start) >= 0. );
			}
			CHECK( frames == static_cast<int>(std::ceil(RATED_SPEED / BASELINE)) );
		}
		THEN( "once stopped it stays stopped" ) {
			for(int i = 0; i < 200; ++i)
				sim.Step(Input());
			CHECK( sim.velocity.Length() < 1e-9 );
		}
	}
	GIVEN( "flight assist is off" ) {
		sim.assist = false;
		THEN( "there is no drag: the ship keeps its velocity" ) {
			for(int i = 0; i < 600; ++i)
				sim.Step(Input());
			CHECK_THAT( sim.velocity.X(), WithinAbs(start.X(), 1e-12) );
			CHECK_THAT( sim.velocity.Y(), WithinAbs(start.Y(), 1e-12) );
		}
	}
}

SCENARIO( "Flight assist caps the speed at the rated speed", "[NewtonianFlight][rated]" ) {
	Sim sim;
	GIVEN( "full thrust from rest" ) {
		THEN( "the speed rises to the rated speed and never above it" ) {
			int framesToRated = 0;
			for(int i = 0; i < 600; ++i)
			{
				sim.Step(Thrust());
				CHECK( sim.velocity.Length() <= RATED_SPEED + 1e-9 );
				if(!framesToRated && sim.velocity.Length() >= RATED_SPEED - 1e-9)
					framesToRated = i + 1;
			}
			CHECK( framesToRated == static_cast<int>(std::ceil(RATED_SPEED / FORWARD)) );
			CHECK_THAT( sim.velocity.Length(), WithinAbs(RATED_SPEED, 1e-9) );
		}
	}
	GIVEN( "the ship is at rated speed and turns 90 degrees" ) {
		sim.velocity = Unit(90.) * RATED_SPEED;
		WHEN( "it thrusts along its new facing" ) {
			sim.Step(Thrust());
			THEN( "it slides: the velocity bends gradually without gaining speed" ) {
				CHECK( sim.velocity.Length() <= RATED_SPEED + 1e-9 );
				CHECK( std::fabs(DeltaTo(90., sim.velocity)) < 5. );
			}
			AND_WHEN( "it keeps thrusting" ) {
				for(int i = 0; i < 300; ++i)
					sim.Step(Thrust());
				THEN( "it ends up flying along its facing at rated speed" ) {
					CHECK( sim.velocity.Unit().Dot(Unit(0.)) > .9999 );
					CHECK_THAT( sim.velocity.Length(), WithinAbs(RATED_SPEED, 1e-9) );
				}
			}
		}
	}
}

SCENARIO( "The afterburner raises the rated speed", "[NewtonianFlight][afterburner]" ) {
	Sim sim;
	const double boostedRated = RATED_SPEED * 1.5;
	GIVEN( "the afterburner is held with thrust" ) {
		sim.rated = boostedRated;
		sim.thrusters = ShipThrusters(.2);
		for(int i = 0; i < 600; ++i)
			sim.Step(Thrust());
		THEN( "the ship reaches the boosted rated speed" ) {
			CHECK_THAT( sim.velocity.Length(), WithinAbs(boostedRated, 1e-9) );
		}
		WHEN( "the afterburner is released but thrust is held" ) {
			sim.rated = RATED_SPEED;
			sim.thrusters = ShipThrusters();
			THEN( "assist brakes back down to the rated speed and holds it there" ) {
				double last = sim.velocity.Length();
				bool sawBraking = false;
				for(int i = 0; i < 600; ++i)
				{
					TranslationPlan plan = sim.Step(Thrust());
					sawBraking |= plan.braking;
					CHECK( sim.velocity.Length() <= last + 1e-9 );
					last = sim.velocity.Length();
				}
				CHECK( sawBraking );
				CHECK_THAT( sim.velocity.Length(), WithinAbs(RATED_SPEED, 1e-9) );
				CHECK_FALSE( sim.Step(Thrust()).braking );
			}
		}
	}
}

SCENARIO( "Flight assist off has a soft cap, not a speed limit", "[NewtonianFlight][softCap]" ) {
	Sim sim;
	sim.assist = false;
	GIVEN( "full thrust from rest" ) {
		THEN( "the speed passes the rated speed and keeps rising, ever more slowly" ) {
			double last = 0.;
			double lastGain = FORWARD;
			double gainAtSoftCap = 0.;
			for(int i = 0; i < 20000 && !gainAtSoftCap; ++i)
			{
				sim.Step(Thrust());
				double speed = sim.velocity.Length();
				double gain = speed - last;
				REQUIRE( gain > 0. );
				CHECK( gain <= lastGain + 1e-12 );
				if(last < RATED_SPEED * SOFT_CAP && speed >= RATED_SPEED * SOFT_CAP)
					gainAtSoftCap = gain;
				last = speed;
				lastGain = gain;
			}
			REQUIRE( gainAtSoftCap > 0. );
			// The factor is taken at the start-of-frame speed, so it's just over 10%.
			CHECK_THAT( gainAtSoftCap / FORWARD, WithinAbs(SOFT_CAP_RESIDUAL, .01) );
		}
	}
	GIVEN( "the ship is far above rated speed" ) {
		sim.velocity = Unit(0.) * RATED_SPEED * 3.;
		WHEN( "it fires its retro thrusters" ) {
			sim.Step(Thrust(-1.));
			THEN( "braking is not soft-capped" ) {
				CHECK_THAT( sim.velocity.Length(), WithinAbs(RATED_SPEED * 3. - BASELINE, 1e-12) );
			}
		}
		WHEN( "STOP is pressed" ) {
			sim.Step(Stop());
			THEN( "nothing happens: STOP needs the flight computer" ) {
				CHECK_THAT( sim.velocity.Length(), WithinAbs(RATED_SPEED * 3., 1e-12) );
			}
		}
	}
}

SCENARIO( "Rotation has angular momentum", "[NewtonianFlight][rotation]" ) {
	Sim sim;
	GIVEN( "flight assist is on" ) {
		THEN( "a full turn input spins up to the rated turn rate and no faster" ) {
			for(int i = 1; i <= 30; ++i)
			{
				sim.Step(Input(), 1.);
				CHECK_THAT( sim.angularVelocity, WithinAbs(std::min(RATED_TURN, i * ANGULAR_ACCEL), 1e-9) );
			}
		}
		WHEN( "the turn input is released" ) {
			for(int i = 0; i < 30; ++i)
				sim.Step(Input(), 1.);
			THEN( "the computer stops the spin within the spin-up time, without reversing" ) {
				for(int i = 0; i < 10; ++i)
				{
					sim.Step(Input());
					CHECK( sim.angularVelocity >= 0. );
				}
				CHECK( sim.angularVelocity == 0. );
			}
		}
	}
	GIVEN( "flight assist is off" ) {
		sim.assist = false;
		for(int i = 0; i < 10; ++i)
			sim.Step(Input(), 1.);
		WHEN( "the turn input is released" ) {
			double omega = sim.angularVelocity;
			for(int i = 0; i < 100; ++i)
				sim.Step(Input());
			THEN( "the ship keeps spinning" ) {
				CHECK( sim.angularVelocity == omega );
			}
		}
		WHEN( "the turn input is held" ) {
			for(int i = 0; i < 100; ++i)
				sim.Step(Input(), 1.);
			THEN( "the spin passes the rated turn rate, but the soft cap slows it" ) {
				CHECK( sim.angularVelocity > RATED_TURN );
				CHECK( sim.angularVelocity < RATED_TURN + 100 * ANGULAR_ACCEL * .5 );
			}
		}
	}
}

SCENARIO( "Turning toward a target angle stops on it", "[NewtonianFlight][turnToward]" ) {
	for(bool assist : {true, false})
		for(double target : {30., 90., 180., -135.})
		{
			DYNAMIC_SECTION( "assist " << (assist ? "on" : "off") << ", target " << target << " degrees" ) {
				Sim sim;
				sim.assist = assist;
				double overshoot = 0.;
				for(int i = 0; i < 600; ++i)
				{
					double delta = remainder(target - sim.angle, 360.);
					double turn = TurnToward(assist, delta, sim.angularVelocity, RATED_TURN, ANGULAR_ACCEL,
						std::fabs(delta) < 1.);
					sim.Step(Input(), turn);
					double after = remainder(target - sim.angle, 360.);
					// Passing the target means the remaining delta changed sign.
					if(after * delta < 0.)
						overshoot = std::max(overshoot, std::fabs(after));
				}
				CHECK( overshoot < 1. );
				CHECK( std::fabs(remainder(target - sim.angle, 360.)) < 1. );
				CHECK( std::fabs(sim.angularVelocity) <= ANGULAR_ACCEL );
			}
		}
}

SCENARIO( "Flip and burn", "[NewtonianFlight][flip]" ) {
	GIVEN( "a fast ship with weak retro thrust" ) {
		const Point velocity = Unit(0.) * 20.;
		THEN( "flipping is faster than braking on the retro thrusters" ) {
			CHECK( ShouldFlipAndBurn(velocity, Unit(0.), 0., ShipBraking()) );
			CHECK( ShouldFlipAndBurn(velocity, Unit(90.), 0., ShipBraking()) );
		}
		THEN( "it isn't when the retro thrusters are as strong as the main engine" ) {
			Braking braking = ShipBraking();
			braking.retro = FORWARD;
			CHECK_FALSE( ShouldFlipAndBurn(velocity, Unit(0.), 0., braking) );
		}
		THEN( "it isn't when the drift is sideways and the lateral thrusters are strong" ) {
			CHECK_FALSE( ShouldFlipAndBurn(velocity, Unit(90.), 0., ShipBraking(FORWARD * 2.)) );
		}
		THEN( "it isn't when one frame of main thrust would stop the ship anyway" ) {
			CHECK_FALSE( ShouldFlipAndBurn(Unit(0.) * FORWARD, Unit(0.), 0., ShipBraking()) );
		}
		THEN( "it isn't when the ship can't turn or has no main engine" ) {
			Braking braking = ShipBraking();
			braking.ratedTurn = 0.;
			CHECK_FALSE( ShouldFlipAndBurn(velocity, Unit(0.), 0., braking) );
			braking = ShipBraking();
			braking.main = 0.;
			CHECK_FALSE( ShouldFlipAndBurn(velocity, Unit(0.), 0., braking) );
		}
	}
	GIVEN( "a fast ship pressing STOP with assist on" ) {
		auto framesToStop = [](bool flip) -> int
		{
			Sim sim;
			sim.velocity = Unit(0.) * 20.;
			// Without a turn rate, the ship can only keep its facing.
			if(!flip)
				sim.ratedTurn = 0.;
			int frames = FlyStop(sim, 2000);
			return sim.velocity ? 0 : frames;
		};
		int flipFrames = framesToStop(true);
		int retroFrames = framesToStop(false);
		THEN( "both come to an exact stop" ) {
			CHECK( flipFrames > 0 );
			CHECK( retroFrames > 0 );
		}
		THEN( "flipping and burning stops it much sooner" ) {
			CHECK( flipFrames < retroFrames / 2 );
		}
	}
}

SCENARIO( "Predicting a stop that keeps the facing", "[NewtonianFlight][predict]" ) {
	// Fly STOP without turning and compare with the prediction, frame for frame.
	auto check = [](const Point &velocity, double facingDegrees, const Braking &braking)
	{
		Sim sim;
		sim.angle = facingDegrees;
		sim.velocity = velocity;
		sim.thrusters.forward = braking.main;
		sim.thrusters.retro = braking.retro;
		sim.thrusters.lateral = braking.lateral;
		StopPrediction prediction = PredictStopKeepingFacing(velocity, Unit(facingDegrees), 0., braking);
		REQUIRE( prediction.canStop );
		CHECK_FALSE( prediction.flip );
		int frames = 0;
		while(sim.velocity && frames < 5000)
		{
			sim.Step(Stop());
			++frames;
		}
		CHECK( frames == prediction.frames );
		CHECK_THAT( sim.position.X(), WithinAbs(prediction.displacement.X(), 1e-6) );
		CHECK_THAT( sim.position.Y(), WithinAbs(prediction.displacement.Y(), 1e-6) );
		return prediction;
	};
	for(double direction : {0., 30., 90., 135., 180., -60.})
		for(double lateral : {BASELINE, BASELINE * 2.})
		{
			DYNAMIC_SECTION( "moving at " << direction << " degrees, lateral thrust " << lateral ) {
				check(Unit(direction) * RATED_SPEED, 0., ShipBraking(lateral));
			}
		}
	GIVEN( "lateral thrust stronger than retro and an oblique drift" ) {
		const Point velocity = Unit(45.) * RATED_SPEED;
		StopPrediction prediction = check(velocity, 0., ShipBraking(BASELINE * 2.));
		THEN( "the ship stops off its velocity line, on the side the weaker thrusters brake" ) {
			double offLine = prediction.displacement.Cross(velocity.Unit());
			CHECK( std::fabs(offLine) > 1. );
			// The sideways part is braked first, so the stop point leans forward of the line.
			CHECK( prediction.displacement.Dot(Unit(0.)) > prediction.displacement.Dot(Unit(90.)) );
		}
	}
	GIVEN( "a ship moving straight backwards" ) {
		StopPrediction prediction = check(Unit(180.) * RATED_SPEED, 0., ShipBraking());
		THEN( "it brakes on the main engine" ) {
			CHECK( prediction.frames == std::ceil(RATED_SPEED / FORWARD) );
		}
	}
	GIVEN( "a ship drifting sideways with no lateral thrust" ) {
		THEN( "it can't stop without turning" ) {
			CHECK_FALSE( PredictStopKeepingFacing(Unit(90.) * 3., Unit(0.), 0., ShipBraking(0.)).canStop );
			StopPrediction stop = PredictStop(Unit(90.) * 3., Unit(0.), 0., ShipBraking(0.));
			CHECK( stop.canStop );
			CHECK( stop.flip );
		}
	}
	GIVEN( "a ship at rest" ) {
		StopPrediction stop = PredictStop(Point(), Unit(0.), 0., ShipBraking());
		THEN( "it is already stopped" ) {
			CHECK( stop.canStop );
			CHECK_FALSE( stop.flip );
			CHECK( stop.frames == 0. );
			CHECK( !stop.displacement );
		}
	}
}

SCENARIO( "Predicting a flip-and-burn stop", "[NewtonianFlight][predict]" ) {
	for(double facing : {0., 60., 120.})
	{
		DYNAMIC_SECTION( "facing " << facing << " degrees from the motion" ) {
			const Point velocity = Unit(0.) * 20.;
			StopPrediction prediction = PredictStop(velocity, Unit(facing), 0., ShipBraking());
			REQUIRE( prediction.flip );
			Sim sim;
			sim.angle = facing;
			sim.velocity = velocity;
			int frames = FlyStop(sim);
			THEN( "the prediction matches the flown stop to within 1%" ) {
				INFO( "flown " << frames << " frames, " << sim.position.Length() << " px; predicted "
					<< prediction.frames << " frames, " << prediction.displacement.Length() << " px" );
				CHECK_THAT( frames, WithinRel(prediction.frames, .01) );
				CHECK_THAT( sim.position.Length(), WithinRel(prediction.displacement.Length(), .01) );
			}
			THEN( "it predicts where the ship comes to rest, off its original line" ) {
				CHECK( sim.position.Unit().Dot(prediction.displacement.Unit()) > .999 );
			}
		}
	}
}

SCENARIO( "Predicting STOP for slow-turning ships", "[NewtonianFlight][predict]" ) {
	// Heavy ships turn slowly and take a long time to spin up. The flip and the
	// held facing then often come out close, and both change as the ship turns,
	// which is why STOP decides once and flies that stop through.
	for(double ratedTurn : {.5, 1., 3.})
		for(double spinUp : {10., 30., 60.})
			for(double facing : {0., 45., 90., 135.})
			{
				DYNAMIC_SECTION( "turning " << ratedTurn << " deg/frame, spin-up " << spinUp
						<< " frames, facing " << facing << " degrees from the motion" ) {
					Sim sim;
					sim.ratedTurn = ratedTurn;
					sim.angularAccel = ratedTurn / spinUp;
					sim.angle = facing;
					sim.velocity = Unit(0.) * 10.;
					StopPrediction prediction = PredictStop(sim.velocity, Unit(facing), 0., SimBraking(sim));
					REQUIRE( prediction.canStop );
					int frames = FlyStop(sim);
					THEN( "the prediction made when STOP begins matches the flown stop" ) {
						INFO( "flown " << frames << " frames, predicted " << prediction.frames
							<< (prediction.flip ? " (flip)" : " (keep facing)") );
						CHECK( !sim.velocity );
						CHECK( std::fabs(frames - prediction.frames) <= prediction.frames * .02 + 1. );
						CHECK( sim.position.Distance(prediction.displacement) <= prediction.displacement.Length() * .02 + 1. );
					}
				}
			}
}

SCENARIO( "Predicting STOP for a ship that is already spinning", "[NewtonianFlight][predict]" ) {
	GIVEN( "a slow-turning ship spinning when it keeps its facing" ) {
		// Assist brakes the spin, and the facing swings round while it does.
		for(double spin : {-1.5, 1.5})
		{
			DYNAMIC_SECTION( "spinning " << spin << " deg/frame" ) {
				Sim sim;
				sim.ratedTurn = 1.5;
				sim.angularAccel = 1.5 / 30.;
				sim.angularVelocity = spin;
				sim.velocity = Unit(30.) * RATED_SPEED;
				sim.thrusters.lateral = BASELINE * 2.;
				StopPrediction prediction = PredictStopKeepingFacing(sim.velocity, Unit(0.), spin, SimBraking(sim));
				StopPrediction still = PredictStopKeepingFacing(sim.velocity, Unit(0.), 0., SimBraking(sim));
				int frames = 0;
				while(sim.velocity && frames < 5000)
				{
					sim.Step(Stop());
					++frames;
				}
				THEN( "the prediction follows the swing, frame for frame" ) {
					CHECK( frames == prediction.frames );
					CHECK_THAT( sim.position.X(), WithinAbs(prediction.displacement.X(), 1e-6) );
					CHECK_THAT( sim.position.Y(), WithinAbs(prediction.displacement.Y(), 1e-6) );
				}
				THEN( "it differs from assuming the facing holds still" ) {
					CHECK( prediction.displacement.Distance(still.displacement) > 1. );
				}
			}
		}
	}
	GIVEN( "a ship already turning to flip" ) {
		const Point velocity = Unit(0.) * 20.;
		const Braking braking = ShipBraking();
		const double fromRest = PredictFlipAndBurn(velocity, Unit(90.), 0., braking).frames;
		THEN( "spinning toward retrograde makes the flip sooner, and away makes it later" ) {
			CHECK( PredictFlipAndBurn(velocity, Unit(90.), RATED_TURN, braking).frames < fromRest );
			CHECK( PredictFlipAndBurn(velocity, Unit(90.), -RATED_TURN, braking).frames > fromRest );
		}
		THEN( "a ship pressing STOP mid-turn is predicted from its current spin" ) {
			Sim sim;
			sim.angle = 90.;
			sim.angularVelocity = RATED_TURN;
			sim.velocity = velocity;
			StopPrediction prediction = PredictStop(sim.velocity, Unit(90.), RATED_TURN, braking);
			REQUIRE( prediction.flip );
			int frames = FlyStop(sim);
			CHECK( std::fabs(frames - prediction.frames) <= prediction.frames * .02 + 1. );
		}
	}
	GIVEN( "a ship with no lateral thrust drifting slowly sideways" ) {
		// Slower than one frame of main thrust, but only a flip can stop it.
		StopPrediction stop = PredictStop(Unit(90.) * (FORWARD * .5), Unit(0.), 0., ShipBraking(0.));
		THEN( "it flips rather than reporting it can't stop" ) {
			CHECK( stop.canStop );
			CHECK( stop.flip );
		}
	}
}

SCENARIO( "Relative motion", "[NewtonianFlight][relative]" ) {
	GIVEN( "a target ahead" ) {
		const Point position(0., -1000.);
		THEN( "closing speed is positive when approaching and negative when opening" ) {
			CHECK_THAT( ClosingSpeed(position, Point(0., 5.)), WithinAbs(5., 1e-12) );
			CHECK_THAT( ClosingSpeed(position, Point(0., -5.)), WithinAbs(-5., 1e-12) );
			CHECK_THAT( ClosingSpeed(position, Point(5., 0.)), WithinAbs(0., 1e-12) );
		}
		THEN( "a target coming straight at us passes at zero distance" ) {
			Approach approach = ClosestApproach(position, Point(0., 5.));
			CHECK_THAT( approach.frames, WithinAbs(200., 1e-9) );
			CHECK_THAT( approach.distance, WithinAbs(0., 1e-9) );
		}
		THEN( "a target crossing our path passes at its offset" ) {
			Approach approach = ClosestApproach(Point(-300., -400.), Point(5., 0.));
			CHECK_THAT( approach.frames, WithinAbs(60., 1e-9) );
			CHECK_THAT( approach.distance, WithinAbs(400., 1e-9) );
		}
		THEN( "a target moving away is closest now" ) {
			Approach approach = ClosestApproach(position, Point(1., -5.));
			CHECK( approach.frames == 0. );
			CHECK_THAT( approach.distance, WithinAbs(1000., 1e-9) );
		}
		THEN( "a target matching our velocity stays where it is" ) {
			Approach approach = ClosestApproach(position, Point());
			CHECK( approach.frames == 0. );
			CHECK_THAT( approach.distance, WithinAbs(1000., 1e-9) );
		}
	}
	GIVEN( "a target at our position" ) {
		THEN( "there is no closing speed" ) {
			CHECK( ClosingSpeed(Point(), Point(3., 4.)) == 0. );
		}
	}
}

SCENARIO( "Emergency station-keeping", "[NewtonianFlight][stationKeeping]" ) {
	GIVEN( "a disabled ship" ) {
		THEN( "it comes nearly to rest in about 8 seconds" ) {
			CHECK( std::pow(STATION_KEEPING_DECAY, 8. * 60.) < .01 );
			CHECK( std::pow(STATION_KEEPING_DECAY, 4. * 60.) > .05 );
		}
	}
}
// #endregion unit tests



} // test namespace
