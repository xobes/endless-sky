/* System.h
Copyright (c) 2014 by Michael Zahniser

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

#include "Point.h"
#include "RaidFleet.h"
#include "RandomEvent.h"
#include "Set.h"
#include "StellarObject.h"
#include "WeightedList.h"

#include <set>
#include <string>
#include <vector>

class ConditionsStore;
class DataNode;
class Date;
class Fleet;
class Government;
class Hazard;
class Minable;
class Outfit;
class Planet;
class Ship;
class Sprite;



// Class representing a star system. This includes characteristics like what
// ships enter that system, what asteroids are present, who owns the system, and
// what prices the trade goods have in that system. It also includes the stellar
// objects in each system, and the hyperspace links between systems.
class System {
public:
	static const double DEFAULT_NEIGHBOR_DISTANCE;

public:
	class Asteroid {
	public:
		Asteroid(const std::string &name, int count, double energy);
		Asteroid(const Minable *type, int count, double energy);

		const std::string &Name() const;
		const Minable *Type() const;
		int Count() const;
		double Energy() const;

	private:
		std::string name;
		const Minable *type = nullptr;
		int count;
		double energy;
	};


public:
	// Load a system's description.
	void Load(const DataNode &node, Set<Planet> &planets, const ConditionsStore *playerConditions);
	// Update any information about the system that may have changed due to events,
	// e.g. neighbors, solar wind and power, or if the system is inhabited.
	void UpdateSystem(const Set<System> &systems, const std::set<double> &neighborDistances);

	// Modify a system's links.
	void Link(System *other);
	void Unlink(System *other);

	bool IsValid() const;
	const std::string &TrueName() const;
	void SetName(const std::string &name);
	// Get this system's name and position (in the star map).
	const std::string &DisplayName() const;
	const Point &Position() const;
	// Get this system's government.
	const Government *GetGovernment() const;
	// Get this system's map icons.
	const std::vector<const Sprite *> &GetMapIcons() const;
	// Get the name of the ambient audio to play in this system.
	const std::string &MusicName() const;

	// Get the list of "attributes" of the planet.
	const std::set<std::string> &Attributes() const;

	// Get a list of systems you can travel to through hyperspace from here.
	const std::set<const System *> &Links() const;
	// Get a list of systems that can be jumped to from here with the given
	// jump distance, whether or not there is a direct hyperspace link to them.
	// If this system has its own jump range, then it will always return the
	// systems within that jump range instead of the jump range given.
	const std::set<const System *> &JumpNeighbors(double neighborDistance) const;
	// Defines whether this system can be seen when not linked. A hidden system will
	// not appear when in view range, except when linked to a visited system.
	bool Hidden() const;
	// Defines whether a system can be remembered when out of view.
	bool Shrouded() const;
	// Defines whether this system can be accessed or interacted with in any way.
	bool Inaccessible() const;
	// Return how much ramscoop fuel is generated by this system,
	// depending on the given ship ramscoop value and the given scale.
	double RamscoopFuel(double shipRamscoop, double scale) const;
	// Additional travel distance to target for ships entering through hyperspace.
	double ExtraHyperArrivalDistance() const;
	// Additional travel distance to target for ships entering using a jumpdrive.
	double ExtraJumpArrivalDistance() const;

	// The minimum distances from the system center to jump out of the system.
	// Separated by jump drives and hyperdrives.
	double JumpDepartureDistance() const;
	double HyperDepartureDistance() const;

	// Get a list of systems you can "see" from here, whether or not there is a
	// direct hyperspace link to them.
	const std::set<const System *> &VisibleNeighbors() const;

	// Move the stellar objects to their positions on the given date.
	void SetDate(const Date &date);
	// Get the stellar object locations on the most recently set date.
	const std::vector<StellarObject> &Objects() const;
	// Get the stellar object (if any) for the given planet.
	const StellarObject *FindStellar(const Planet *planet) const;
	// Get the habitable zone's center.
	double HabitableZone() const;
	// Get the radius of an asteroid belt.
	double AsteroidBeltRadius() const;
	// Get the list of asteroid belts.
	const WeightedList<double> &AsteroidBelts() const;
	// Get the system's invisible fence radius.
	double InvisibleFenceRadius() const;
	// Get how far ships can jump from this system.
	double JumpRange() const;
	// Get the rate of solar collection and ramscoop refueling.
	double SolarPower() const;
	double SolarWind() const;
	// Get the starfield density for this system.
	double StarfieldDensity() const;
	// Check if this system is inhabited.
	bool IsInhabited(const Ship *ship) const;
	// Check if ships of the given government can refuel in this system.
	bool HasFuelFor(const Ship &ship) const;
	// Check whether you can buy or sell ships in this system.
	bool HasShipyard() const;
	// Check whether you can buy or sell ship outfits in this system.
	bool HasOutfitter() const;

	// Get the specification of how many asteroids of each type there are.
	const std::vector<Asteroid> &Asteroids() const;
	// Get a list of all unique payload outfits from minables in this system.
	const std::set<const Outfit *> &Payloads() const;
	// Get the background haze sprite for this system.
	const Sprite *Haze() const;

	// Get the price of the given commodity in this system.
	int Trade(const std::string &commodity) const;
	bool HasTrade() const;
	// Update the economy. Returns the amount of trade goods this system exports.
	void StepEconomy();
	void SetSupply(const std::string &commodity, double tons);
	double Supply(const std::string &commodity) const;
	double Exports(const std::string &commodity) const;

	// Get the probabilities of various fleets entering this system.
	const std::vector<RandomEvent<Fleet>> &Fleets() const;
	// Get the probabilities of various hazards in this system.
	const std::vector<RandomEvent<Hazard>> &Hazards() const;
	// Check how dangerous this system is (credits worth of enemy ships jumping
	// in per frame).
	double Danger() const;

	// The smallest arrival period of a fleet (or 0 if no fleets arrive)
	int MinimumFleetPeriod() const;

	const std::vector<RaidFleet> &RaidFleets() const;


private:
	void LoadObject(const DataNode &node, Set<Planet> &planets,
		const ConditionsStore *playerConditions, int parent = -1);
	void LoadObjectHelper(const DataNode &node, StellarObject &object, bool removing = false) const;
	// Once the star map is fully loaded or an event has changed systems
	// or links, figure out which stars are "neighbors" of this one, i.e.
	// close enough to see or to reach via jump drive.
	void UpdateNeighbors(const Set<System> &systems, double distance);


private:
	class Price {
	public:
		void SetBase(int base);
		void Update();

		int base = 0;
		int price = 0;
		double supply = 0.;
		double exports = 0.;
	};


private:
	bool isDefined = false;
	bool hasPosition = false;
	std::string trueName;
	// Name and position (within the star map) of this system.
	std::string displayName;
	Point position;
	const Government *government = nullptr;
	std::vector<const Sprite *> mapIcons;
	std::string music;

	// All possible hyperspace links to other systems.
	std::set<const System *> links;
	// Only those hyperspace links to other systems that are accessible.
	std::set<const System *> accessibleLinks;
	// Other systems that can be accessed from this system via a jump drive at various jump ranges.
	std::map<double, std::set<const System *>> neighbors;

	// Defines whether this system can be seen when not linked. A hidden system will
	// not appear when in view range, except when linked to a visited system.
	bool hidden = false;
	// Defines whether a system can be remembered when out of view.
	bool shrouded = false;
	// Defines whether this system can be accessed or interacted with in any way.
	bool inaccessible = false;

	// Defines whether this system provides ramscoop even to ships that do not have any.
	bool universalRamscoop = true;
	// A value that is added to the ramscoop. It can be positive or negative.
	double ramscoopAddend = 0.;
	// A multiplier applied to ramscoop in the system.
	double ramscoopMultiplier = 1.;

	// Stellar objects, listed in such an order that an object's parents are
	// guaranteed to appear before it (so that if we traverse the vector in
	// order, updating positions, an object's parents will already be at the
	// proper position before that object is updated).
	std::vector<StellarObject> objects;
	std::vector<Asteroid> asteroids;
	std::set<const Outfit *> payloads;
	const Sprite *haze = nullptr;
	std::vector<RandomEvent<Fleet>> fleets;
	std::vector<RandomEvent<Hazard>> hazards;
	double habitable = 1000.;
	WeightedList<double> belts;
	double invisibleFenceRadius = 10000.;
	double jumpRange = 0.;
	double solarPower = 0.;
	double solarWind = 0.;
	double starfieldDensity = 1.;
	int minimumFleetPeriod = 0;

	std::vector<RaidFleet> raidFleets;
	bool noRaids = false;

	// The amount of additional distance that ships will arrive away from the
	// system center when entering this system through a hyperspace link.
	// Negative values are allowed, causing ships to jump beyond their target.
	double extraHyperArrivalDistance = 0.;
	// The amount of additional distance that ships will arrive away from the
	// system center when entering this system through a jumpdrive jump.
	// Jump drives use a circle around the target for targeting, so a value below
	// 0 doesn't have the same meaning as for hyperdrives. Negative values will
	// be interpreted as positive values.
	double extraJumpArrivalDistance = 0.;

	// The minimum distances from the system center to jump out of the system.
	double jumpDepartureDistance = 0.;
	double hyperDepartureDistance = 0.;

	// Commodity prices.
	std::map<std::string, Price> trade;

	// Attributes, for use in location filters.
	std::set<std::string> attributes;
};
