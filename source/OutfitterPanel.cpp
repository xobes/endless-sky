/* OutfitterPanel.cpp
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

#include "OutfitterPanel.h"

#include "text/alignment.hpp"
#include "comparators/BySeriesAndIndex.h"
#include "Color.h"
#include "Dialog.h"
#include "text/DisplayText.h"
#include "FillShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "text/Format.h"
#include "GameData.h"
#include "Hardpoint.h"
#include "Mission.h"
#include "Outfit.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Point.h"
#include "Screen.h"
#include "Ship.h"
#include "image/Sprite.h"
#include "image/SpriteSet.h"
#include "SpriteShader.h"
#include "text/truncate.hpp"
#include "UI.h"

#include <algorithm>
#include <limits>
#include <memory>

using namespace std;

namespace {
	// Label for the description field of the detail pane.
	const string DESCRIPTION = "description";

	// Determine the refillable ammunition a particular ship consumes or stores.
	set<const Outfit *> GetRefillableAmmunition(const Ship &ship) noexcept
	{
		auto toRefill = set<const Outfit *>{};
		auto armed = set<const Outfit *>{};
		for(auto &&it : ship.Weapons())
			if(it.GetOutfit())
			{
				const Outfit *weapon = it.GetOutfit();
				armed.emplace(weapon);
				if(weapon->Ammo() && weapon->AmmoUsage() > 0)
					toRefill.emplace(weapon->Ammo());
			}

		// Carriers may be configured to supply ammunition for carried ships found
		// within the fleet. Since a particular ammunition outfit is not bound to
		// any particular weapon (i.e. one weapon may consume it, while another may
		// only require it be installed), we always want to restock these outfits.
		for(auto &&it : ship.Outfits())
		{
			const Outfit *outfit = it.first;
			if(outfit->Ammo() && !outfit->IsWeapon() && !armed.contains(outfit))
				toRefill.emplace(outfit->Ammo());
		}
		return toRefill;
	}
}



OutfitterPanel::OutfitterPanel(PlayerInfo &player)
	: ShopPanel(player, true)
{
	for(const pair<const string, Outfit> &it : GameData::Outfits())
		catalog[it.second.Category()].push_back(it.first);

	for(pair<const string, vector<string>> &it : catalog)
		sort(it.second.begin(), it.second.end(), BySeriesAndIndex<Outfit>());

	if(player.GetPlanet())
		outfitter = player.GetPlanet()->Outfitter();

	for(auto &ship : player.Ships())
		if(ship->GetPlanet() == planet)
			++shipsHere;
}



void OutfitterPanel::Step()
{
	CheckRefill();
	ShopPanel::Step();
	ShopPanel::CheckForMissions(Mission::OUTFITTER);
	if(GetUI()->IsTop(this) && !checkedHelp)
		// Use short-circuiting to only display one of them at a time.
		// (The first valid condition encountered will make us skip the others.)
		if(DoHelp("outfitter") || DoHelp("cargo management") || DoHelp("uninstalling and storage")
				|| (shipsHere > 1 && DoHelp("outfitter with multiple ships")) || true)
			// Either a help message was freshly displayed, or all of them have already been seen.
			checkedHelp = true;
}



int OutfitterPanel::TileSize() const
{
	return OUTFIT_SIZE;
}



int OutfitterPanel::VisibilityCheckboxesSize() const
{
	return 80; // checkboxHeight = 20 x 4 checkboxes = 80; 
}



bool OutfitterPanel::HasItem(const string &name) const
{
	const Outfit *outfit = GameData::Outfits().Get(name);
	if(showForSale && (outfitter.Has(outfit) || player.Stock(outfit) > 0))
		return true;

	if(showCargo && player.Cargo().Get(outfit))
		return true;

	if(showStorage && player.Storage().Get(outfit))
		return true;

	if(showInstalled)
		for(const Ship *ship : playerShips)
			if(ship->OutfitCount(outfit))
				return true;

	if(showForSale && HasLicense(name))
		return true;

	return false;
}



void OutfitterPanel::DrawItem(const string &name, const Point &point)
{
	const Outfit *outfit = GameData::Outfits().Get(name);
	zones.emplace_back(point, Point(OUTFIT_SIZE, OUTFIT_SIZE), outfit);
	if(point.Y() + OUTFIT_SIZE / 2 < Screen::Top() || point.Y() - OUTFIT_SIZE / 2 > Screen::Bottom())
		return;

	bool isSelected = (outfit == selectedOutfit);
	bool isOwned = playerShip && playerShip->OutfitCount(outfit);
	DrawOutfit(*outfit, point, isSelected, isOwned);

	// Check if this outfit is a "license".
	bool isLicense = IsLicense(name);
	int mapSize = outfit->Get("map");

	const Font &font = FontSet::Get(14);
	const Color &bright = *GameData::Colors().Get("bright");
	if(playerShip || isLicense || mapSize)
	{
		int minCount = numeric_limits<int>::max();
		int maxCount = 0;
		if(isLicense)
			minCount = maxCount = player.HasLicense(LicenseRoot(name));
		else if(mapSize)
			minCount = maxCount = player.HasMapped(mapSize);
		else
		{
			for(const Ship *ship : playerShips)
			{
				int count = ship->OutfitCount(outfit);
				minCount = min(minCount, count);
				maxCount = max(maxCount, count);
			}
		}

		if(maxCount)
		{
			string label = "installed: " + to_string(minCount);
			if(maxCount > minCount)
				label += " - " + to_string(maxCount);

			Point labelPos = point + Point(-OUTFIT_SIZE / 2 + 20, OUTFIT_SIZE / 2 - 38);
			font.Draw(label, labelPos, bright);
		}
	}
	// Don't show the "in stock" amount if the outfit has an unlimited stock.
	int stock = 0;
	if(!outfitter.Has(outfit))
		stock = max(0, player.Stock(outfit));
	int cargo = player.Cargo().Get(outfit);
	int storage = player.Storage().Get(outfit);

	string message;
	if(cargo && storage && stock)
		message = "cargo+stored: " + to_string(cargo + storage) + ", in stock: " + to_string(stock);
	else if(cargo && storage)
		message = "in cargo: " + to_string(cargo) + ", in storage: " + to_string(storage);
	else if(cargo && stock)
		message = "in cargo: " + to_string(cargo) + ", in stock: " + to_string(stock);
	else if(storage && stock)
		message = "in storage: " + to_string(storage) + ", in stock: " + to_string(stock);
	else if(cargo)
		message = "in cargo: " + to_string(cargo);
	else if(storage)
		message = "in storage: " + to_string(storage);
	else if(stock)
		message = "in stock: " + to_string(stock);
	else if(!outfitter.Has(outfit))
		message = "(not sold here)";
	if(!message.empty())
	{
		Point pos = point + Point(
			OUTFIT_SIZE / 2 - 20 - font.Width(message),
			OUTFIT_SIZE / 2 - 24);
		font.Draw(message, pos, bright);
	}
}



double OutfitterPanel::ButtonPanelHeight() const
{
	// The 60 is for padding and the Info lines
	return 60. + BUTTON_HEIGHT * 2 + BUTTON_ROW_PAD;
}



int OutfitterPanel::DetailWidth() const
{
	return 3 * ItemInfoDisplay::PanelWidth();
}



double OutfitterPanel::DrawDetails(const Point &center)
{
	string selectedItem = "Nothing Selected";
	const Font &font = FontSet::Get(14);

	double heightOffset = 20.;

	if(selectedOutfit)
	{
		outfitInfo.Update(*selectedOutfit, player, static_cast<bool>(CanSell()), collapsed.contains(DESCRIPTION));
		selectedItem = selectedOutfit->DisplayName();

		const Sprite *thumbnail = selectedOutfit->Thumbnail();
		const float tileSize = thumbnail
			? max(thumbnail->Height(), static_cast<float>(TileSize()))
			: static_cast<float>(TileSize());
		const Point thumbnailCenter(center.X(), center.Y() + 20 + static_cast<int>(tileSize / 2));
		const Point startPoint(center.X() - INFOBAR_WIDTH / 2 + 20, center.Y() + 20 + tileSize);

		const Sprite *background = SpriteSet::Get("ui/outfitter selected");
		SpriteShader::Draw(background, thumbnailCenter);
		if(thumbnail)
			SpriteShader::Draw(thumbnail, thumbnailCenter);

		const bool hasDescription = outfitInfo.DescriptionHeight();

		double descriptionOffset = hasDescription ? 40. : 0.;

		if(hasDescription)
		{
			if(!collapsed.contains(DESCRIPTION))
			{
				descriptionOffset = outfitInfo.DescriptionHeight();
				outfitInfo.DrawDescription(startPoint);
			}
			else
			{
				const Color &dim = *GameData::Colors().Get("medium");
				font.Draw(DESCRIPTION, startPoint + Point(35., 12.), dim);
				const Sprite *collapsedArrow = SpriteSet::Get("ui/collapsed");
				SpriteShader::Draw(collapsedArrow, startPoint + Point(20., 20.));
			}

			// Calculate the ClickZone for the description and add it.
			const Point descriptionDimensions(INFOBAR_WIDTH, descriptionOffset);
			const Point descriptionCenter(center.X(), startPoint.Y() + descriptionOffset / 2);
			ClickZone<string> collapseDescription = ClickZone<string>(
				descriptionCenter, descriptionDimensions, DESCRIPTION);
			categoryZones.emplace_back(collapseDescription);
		}

		const Point requirementsPoint(startPoint.X(), startPoint.Y() + descriptionOffset);
		const Point attributesPoint(startPoint.X(), requirementsPoint.Y() + outfitInfo.RequirementsHeight());
		outfitInfo.DrawRequirements(requirementsPoint);
		outfitInfo.DrawAttributes(attributesPoint);

		heightOffset = attributesPoint.Y() + outfitInfo.AttributesHeight();
	}

	// Draw this string representing the selected item (if any), centered in the details side panel
	const Color &bright = *GameData::Colors().Get("bright");
	Point selectedPoint(center.X() - INFOBAR_WIDTH / 2, center.Y());
	font.Draw({selectedItem, {INFOBAR_WIDTH, Alignment::CENTER, Truncate::MIDDLE}},
		selectedPoint, bright);

	return heightOffset;
}



bool OutfitterPanel::IsInShop() const
{
	return outfitter.Has(selectedOutfit) || player.Stock(selectedOutfit) > 0;
}



// Can the selected outfit be purchased (availability, payment, licensing)
ShopPanel::TransactionResult OutfitterPanel::CanPurchase() const
{
	if(!planet || !selectedOutfit)
		return "No outfit selected.";

	if(!IsInShop())
	{
		// The store doesn't have it.
		return "You cannot buy this outfit here. "
			"It is being shown in the list because you have one, "
			"but this " + planet->Noun() + " does not sell them.";
	}

	// Check special unique outfits, if you already have them.
	int mapSize = selectedOutfit->Get("map");
	if(mapSize > 0 && player.HasMapped(mapSize))
		return "You have already mapped all the systems shown by this map, "
			"so there is no reason to buy another.";

	if(HasLicense(selectedOutfit->TrueName()))
		return "You already have one of these licenses, "
			"so there is no reason to buy another.";

	// Determine what you will have to pay to buy this outfit.
	int64_t cost = player.StockDepreciation().Value(selectedOutfit, day);
	int64_t credits = player.Accounts().Credits();

	if(cost > credits)
		return "You don't have enough money to buy this outfit, you need a further " +
			Format::CreditString(cost - credits);

	// Add the cost to buy the required license.
	int64_t licenseCost = LicenseCost(selectedOutfit, false);
	if(cost + licenseCost > credits)
		return "You don't have enough money to buy this outfit because you also need to buy a "
			"license for it. You need a further " +
			Format::CreditString(licenseCost - credits);

	// Check that the player has any necessary licenses.
	if(LicenseCost(selectedOutfit, false) < 0)
		return "You cannot buy this outfit, because it requires a license that you don't have.";

	// The outfit is able to be purchased (available in the Outfitter, licensed and affordable)
	return true;
}



// Checking where it will go (can it fit in fleet cargo), not where it will come from
ShopPanel::TransactionResult OutfitterPanel::CanFitInCargo() const
{
	// TODO: https://github.com/endless-sky/endless-sky/issues/10599

	// Check fleet cargo space vs mass.
	double mass = selectedOutfit->Mass();
	double freeCargo = player.Cargo().FreePrecise();
	if(!mass || freeCargo >= mass)
		return true;

	return "You cannot load this outfit into cargo, because it takes up "
		+ Format::CargoString(mass, "mass") + " and your fleet has "
		+ Format::CargoString(freeCargo, "cargo space") + " free.";
}



// Checking where it will go (can actually be installed into ANY ship), not where it will come from
ShopPanel::TransactionResult OutfitterPanel::CanBeInstalled() const
{
	if(!planet || !selectedOutfit)
		return "No outfit selected.";

	if(!playerShip)
		return "No ship selected.";

	// Collect relevant errors
	vector<string> errors;

	// Find if any ship can install the outfit.
	for(const Ship *ship : playerShips)
		if(ShipCanAdd(ship, selectedOutfit))
			return true;

	// If no selected ship can install the outfit, report error based on playerShip.
	double outfitNeeded = -selectedOutfit->Get("outfit space");
	double outfitSpace = playerShip->Attributes().Get("outfit space");
	if(outfitNeeded > outfitSpace)
		errors.push_back("You cannot install this outfit, because it takes up "
			+ Format::CargoString(outfitNeeded, "outfit space") + ", and this ship has "
			+ Format::MassString(outfitSpace) + " free.");

	double weaponNeeded = -selectedOutfit->Get("weapon capacity");
	double weaponSpace = playerShip->Attributes().Get("weapon capacity");
	if(weaponNeeded > weaponSpace)
		errors.push_back("Only part of your ship's outfit capacity is usable for weapons. "
			"You cannot install this outfit, because it takes up "
			+ Format::CargoString(weaponNeeded, "weapon space") + ", and this ship has "
			+ Format::MassString(weaponSpace) + " free.");

	double engineNeeded = -selectedOutfit->Get("engine capacity");
	double engineSpace = playerShip->Attributes().Get("engine capacity");
	if(engineNeeded > engineSpace)
		errors.push_back("Only part of your ship's outfit capacity is usable for engines. "
			"You cannot install this outfit, because it takes up "
			+ Format::CargoString(engineNeeded, "engine space") + ", and this ship has "
			+ Format::MassString(engineSpace) + " free.");

	if(selectedOutfit->Category() == "Ammunition")
		errors.emplace_back(!playerShip->OutfitCount(selectedOutfit) ?
			"This outfit is ammunition for a weapon. "
			"You cannot install it without first installing the appropriate weapon."
			: "You already have the maximum amount of ammunition for this weapon. "
			"If you want to install more ammunition, you must first install another of these weapons.");

	int mountsNeeded = -selectedOutfit->Get("turret mounts");
	int mountsFree = playerShip->Attributes().Get("turret mounts");
	if(mountsNeeded && !mountsFree)
		errors.emplace_back("This weapon is designed to be installed on a turret mount, "
			"but your ship does not have any unused turret mounts available.");

	int gunsNeeded = -selectedOutfit->Get("gun ports");
	int gunsFree = playerShip->Attributes().Get("gun ports");
	if(gunsNeeded && !gunsFree)
		errors.emplace_back("This weapon is designed to be installed in a gun port, "
			"but your ship does not have any unused gun ports available.");

	if(selectedOutfit->Get("installable") < 0.)
		errors.emplace_back("This item is not an outfit that can be installed in a ship.");

	// For unhandled outfit requirements, show a catch-all error message.
	if(errors.empty())
		errors.emplace_back("You cannot install this outfit in your ship, "
			"because it would reduce one of your ship's attributes to a negative amount.");

	// return the errors in the appropriate format
	if(errors.empty())
		return true;

	if(errors.size() == 1)
		return errors[0];

	string errorMessage = "There are several reasons why you cannot buy this outfit:\n";
	for(const auto & error : errors)
		errorMessage += "- " + error + "\n";

	return errorMessage;
}



// Used in both the Sell and the Uninstall contexts, are we able to remove Outfits from
// the selected ships, return reasons why not
ShopPanel::TransactionResult OutfitterPanel::CanSellOrUninstall(const string &verb) const
{
	if(!planet || !selectedOutfit)
		return "No outfit selected.";

	if(!playerShip)
		return "No ship selected.";

	// Collect the reasons that the outfit can't be sold or uninstalled
	vector<string> errors;

	if(static_cast<bool>(selectedOutfit->Get("map")))
		errors.push_back("You cannot " + verb + " maps. Once you buy one, it is yours permanently.");
	else if(HasLicense(selectedOutfit->TrueName()))
		errors.push_back("You cannot " + verb + " licenses. Once you obtain one, it is yours permanently.");
	else
	{
		bool hasOutfit = false;
		for(const Ship *ship : playerShips)
			if(ship->OutfitCount(selectedOutfit))
			{
				hasOutfit = true;
				break;
			}
		if(!hasOutfit)
			errors.push_back("You don't have any of these outfits to " + verb + ".");
		else
		{
			for(const Ship *ship : playerShips)
				for(const pair<const char *, double> &it : selectedOutfit->Attributes())
					if(ship->Attributes().Get(it.first) < it.second)
					{
						bool detailsFound = false;
						for(const auto &sit : ship->Outfits())
							if(sit.first->Get(it.first) < 0.)
							{
								errors.push_back("You cannot " + verb + " this outfit, "
									"because that would cause your ship " + ship->Name() + "'s " +
									"\"" + it.first + "\" value to be reduced to less than zero. " +
									"To " + verb + " this outfit, you must " + verb + " the " +
									sit.first->DisplayName() + " outfit first.");
								detailsFound = true;
							}
						if(!detailsFound)
						{
							errors.push_back("You cannot " + verb + " this outfit, "
								"because that would cause your ship " + ship->Name() + "'s \"" +
								it.first + "\" value to be reduced to less than zero.");
						}
					}
		}
	}

	// return the errors in the appropriate format
	if(errors.empty())
		return true;

	if(errors.size() == 1)
		return errors[0];

	string errorMessage = "There are several reasons why you cannot " + verb + " this outfit:\n";
	for(const auto & error : errors)
		errorMessage += "- " + error + "\n";
	return errorMessage;
}



// Uninstall outfits from selected ships, handles ammo dependencies
void OutfitterPanel::SellOrUninstall(SDL_Keycode key) const
{
	// Key:
	//   's' - Sell
	//   'u' - Uninstall
	//   'r' - Move to Storage
	// Get the ships that have the most of this outfit installed.
	if(const vector<Ship *> shipsToOutfit = GetShipsToOutfit(); !shipsToOutfit.empty())
	{
		// Note: to get here, we have already confirmed that every ship in the selection
		// has the outfit and it is able to be uninstalled in the first place
		for(Ship *ship : shipsToOutfit)
		{
			// Uninstall the outfit
			ship->AddOutfit(selectedOutfit, -1);
			if(selectedOutfit->Get("required crew"))
				ship->AddCrew(-selectedOutfit->Get("required crew"));
			ship->Recharge();

			// context is sale
			if(key == 's')
			{
				// Do the Sale
				int64_t price = player.FleetDepreciation().Value(selectedOutfit, day);
				player.Accounts().AddCredits(price);
				player.AddStock(selectedOutfit, 1);
			}
			// context is uninstall, Move the outfit into Cargo if we can, so we can take it with us if possible
			else if(key == 'u' && CanFitInCargo())
			{
				// move to cargo
				player.Cargo().Add(selectedOutfit, 1);
			}
			// context is move to storage ('r'), or sale/uninstall
			// couldn't make it work with Cargo, leave it on the planet (by definition this planet has an outfitter)
			else
				// move to storage
				player.Storage().Add(selectedOutfit, 1);

			// Since some outfits have ammo, remove any ammo that must be sold because there
			// aren't enough supporting slots for said ammo once this outfit is removed.
			const Outfit *ammo = selectedOutfit->Ammo();
			if(ammo && ship->OutfitCount(ammo))
			{
				// Determine how many of this ammo we must Uninstall to also Uninstall the launcher.
				int mustUninstall = 0;
				for(const pair<const char *, double> &it : ship->Attributes().Attributes())
					if(it.second < 0.)
						mustUninstall = max<int>(mustUninstall, it.second / ammo->Get(it.first));

				if(mustUninstall)
				{
					ship->AddOutfit(ammo, -mustUninstall);

					// context is sale
					if(key == 's')
					{
						// Do the sale
						int64_t price = player.FleetDepreciation().Value(ammo, day, mustUninstall);
						player.Accounts().AddCredits(price);
						player.AddStock(ammo, mustUninstall);
					}
					// context is uninstall, Move the outfit into Cargo if we can, so we can take it with us if possible
					else if(key == 'u' && CanFitInCargo())
					{
						// move to cargo
						player.Cargo().Add(ammo, mustUninstall);
					}
					// context is move to storage ('r'), or sale/uninstall
					// couldn't make it work with Cargo, leave it on the planet (by definition this planet has an outfitter)
					else
						// move to storage
						player.Storage().Add(ammo, mustUninstall);
				}
			}
		}
	}
}



// Check if the outfit can be purchased (availability, payment, licensing), and
// if it can be installed, return reasons why not
ShopPanel::TransactionResult OutfitterPanel::CanBuy() const
{
	if(TransactionResult result = CanPurchase(); !result)
		return result;
	return CanBeInstalled();
}



// Perform Buy from shop, optionally to Cargo directly
void OutfitterPanel::Buy(bool toCargo) const
{
	// by definition Buy is from Outfitter and Stock
	if(int64_t licenseCost = LicenseCost(selectedOutfit, false))
	{
		player.Accounts().AddCredits(-licenseCost);
		for(const string &licenseName : selectedOutfit->Licenses())
			if(!player.HasLicense(licenseName))
				player.AddLicense(licenseName);
	}

	// Special case: maps.
	int mapSize = selectedOutfit->Get("map");
	if(mapSize)
	{
		player.Map(mapSize);
		player.Accounts().AddCredits(-selectedOutfit->Cost());
		return;
	}

	// Special case: licenses.
	if(IsLicense(selectedOutfit->TrueName()))
	{
		player.AddLicense(LicenseRoot(selectedOutfit->TrueName()));
		player.Accounts().AddCredits(-selectedOutfit->Cost());
		return;
	}

	int modifier = Modifier();
	for(int i = 0; i < modifier && (CanBuy() || (toCargo && CanBuyToCargo())); ++i)
	{
		// Buying into cargo, either from storage (free!) or from stock/supply.
		if(toCargo)
		{
			if(player.Storage().Get(selectedOutfit))
			{
				// found in storage, it's free!
				player.Cargo().Add(selectedOutfit);
				player.Storage().Remove(selectedOutfit);
			}
			else
			{
				// Check if the outfit is for sale or in stock so that we can actually buy it.
				if(IsInShop())
				{
					player.Cargo().Add(selectedOutfit);
					int64_t price = player.StockDepreciation().Value(selectedOutfit, day);
					player.Accounts().AddCredits(-price);
					player.AddStock(selectedOutfit, -1);
				}
			}
		}
		else
		{
			// Find the ships with the fewest number of these outfits.
			const vector<Ship *> shipsToOutfit = GetShipsToOutfit(true);
			for(Ship *ship : shipsToOutfit)
			{
				if(!CanBuy())
					return;

				// use cargo first
				if(player.Cargo().Get(selectedOutfit))
					player.Cargo().Remove(selectedOutfit);

				// use storage second
				else if(player.Storage().Get(selectedOutfit))
					player.Storage().Remove(selectedOutfit);

				// buy if there aren't any left to buy, bail
				else if(!IsInShop())
					return;

				// buy from the Outfitter
				else
				{
					int64_t price = player.StockDepreciation().Value(selectedOutfit, day);
					player.Accounts().AddCredits(-price);
					player.AddStock(selectedOutfit, -1);
				}

				// now Install it on this ship
				ship->AddOutfit(selectedOutfit, 1);
				int required = selectedOutfit->Get("required crew");
				if(required && ship->Crew() + required <= static_cast<int>(ship->Attributes().Get("bunks")))
					ship->AddCrew(required);
				ship->Recharge();
			}
		}
	}
}



// Buy and Install
void OutfitterPanel::Buy()
{
	Buy(false);
}



// Check if the inputs and outputs are right to Buy directly into Cargo
ShopPanel::TransactionResult OutfitterPanel::CanBuyToCargo() const
{
	TransactionResult canPurchase = CanPurchase();
	if(!canPurchase)
		return canPurchase;
	return CanFitInCargo();
}



// Buy outfits directly into Cargo
void OutfitterPanel::BuyToCargo()
{
	Buy(true);
}



// Check if this outfit can be sold, return reasons why not
ShopPanel::TransactionResult OutfitterPanel::CanSell() const
{
	if(!planet || !selectedOutfit)
		return "No outfit selected.";

	// Can sell things in Cargo
	if(player.Cargo().Get(selectedOutfit))
		return true;

	// Can sell things in Storage
	if(player.Storage().Get(selectedOutfit))
		return true;

	// There are reasons that Selling may fail related to availability of outfits
	// on ships based on ship selection
	return CanSellOrUninstall("sell");
}



// Sell outfits from Cargo, then Storage, then Ships
void OutfitterPanel::Sell()
{
	// Now figure out where to sell it from
	// Cargo first
	if(player.Cargo().Get(selectedOutfit))
	{
		player.Cargo().Remove(selectedOutfit);

		// Do the sale
		int64_t price = player.FleetDepreciation().Value(selectedOutfit, day);
		player.Accounts().AddCredits(price);
		player.AddStock(selectedOutfit, 1);
	}
	// Storage second
	else if(player.Storage().Get(selectedOutfit))
	{
		player.Storage().Remove(selectedOutfit);

		// Do the sale
		int64_t price = player.FleetDepreciation().Value(selectedOutfit, day);
		player.Accounts().AddCredits(price);
		player.AddStock(selectedOutfit, 1);
	}
	// And lastly, from one of EACH of the selected Ships
	// Get the ships that have the most of this outfit installed.
	else if(const vector<Ship *> shipsToOutfit = GetShipsToOutfit(); !shipsToOutfit.empty())
	{
		SellOrUninstall('s');
	}
}



// Check if the outfit is available to Install (from already owned stores), return reasons why not
ShopPanel::TransactionResult OutfitterPanel::CanInstall() const
{
	if(!planet || !selectedOutfit)
		return "No outfit selected.";

	if(!player.Storage().Get(selectedOutfit) && !player.Cargo().Get(selectedOutfit))
		return "You don't have any of this outfit to install, use \"B\" to buy (and install) it.";

	// And then check if installation requirements are met
	return CanBeInstalled();
}



// Install from already owned stores: Cargo or Storage
void OutfitterPanel::Install()
{
	int modifier = Modifier();
	for(int i = 0; i < modifier && CanBeInstalled(); ++i)
	{
		// Find the ships with the fewest number of these outfits.
		const vector<Ship *> shipsToOutfit = GetShipsToOutfit(true);
		for(Ship *ship : shipsToOutfit)
		{
			if(!CanBeInstalled())
				return;

			// use cargo first
			if(player.Cargo().Get(selectedOutfit))
				player.Cargo().Remove(selectedOutfit);

			// use storage second
			else if(player.Storage().Get(selectedOutfit))
				player.Storage().Remove(selectedOutfit);

			// if there aren't any left, bail
			else
				return;

			// Install it
			ship->AddOutfit(selectedOutfit, 1);
			int required = selectedOutfit->Get("required crew");
			if(required && ship->Crew() + required <= static_cast<int>(ship->Attributes().Get("bunks")))
				ship->AddCrew(required);
			ship->Recharge();
		}
	}
}



// Check if the outfit is able to be uninstalled, return reasons why not
ShopPanel::TransactionResult OutfitterPanel::CanUninstall() const
{
	return CanSellOrUninstall("uninstall");
}


// Check if the outfit is able to be uninstalled, return reasons why not
void OutfitterPanel::Uninstall()
{
	return SellOrUninstall('u');
}



// Check if the outfit is able to be moved to Cargo from Storage, return reasons why not
ShopPanel::TransactionResult OutfitterPanel::CanMoveToCargo() const
{
	if(!planet || !selectedOutfit)
		return "No outfit selected.";

	if(static_cast<bool>(selectedOutfit->Get("map")))
		return "You cannot move maps around. Once you buy one, it is yours permanently.";

	if(HasLicense(selectedOutfit->TrueName()))
		return "You cannot move licenses around. Once you obtain one, it is yours permanently.";

	if(!player.Storage().Get(selectedOutfit))
		return "You don't have any of these outfits in storage to move to your cargo hold.";

	return CanFitInCargo();
}



// Move to Cargo from Storage, only (no uninstalling!)
void OutfitterPanel::MoveToCargo()
{
	player.Cargo().Add(selectedOutfit);
	player.Storage().Remove(selectedOutfit);
}



// Check if the outfit is able to be moved to Storage from Cargo or Ship, return reasons why not
ShopPanel::TransactionResult OutfitterPanel::CanMoveToStorage() const
{
	if(!planet || !selectedOutfit)
		return "No outfit selected.";

	if(static_cast<bool>(selectedOutfit->Get("map")))
		return "You cannot move maps around. Once you buy one, it is yours permanently.";

	if(HasLicense(selectedOutfit->TrueName()))
		return "You cannot move licenses around. Once you obtain one, it is yours permanently.";

	// if we have one in cargo, move that one
	if(player.Cargo().Get(selectedOutfit))
		return true;

	// otherwise it depends on if it can actually be uninstalled
	return CanSellOrUninstall("move to storage");
}



// Move to Storage from Cargo or Ship
void OutfitterPanel::MoveToStorage()
{
	// from cargo first, if available -- from fleet as a whole
	if(player.Cargo().Get(selectedOutfit))
	{
		player.Cargo().Remove(selectedOutfit);
		player.Storage().Add(selectedOutfit);
	}
	// else, uninstall and move to storage -- from each selected ship
	else
		SellOrUninstall('r');
}



bool OutfitterPanel::ShouldHighlight(const Ship *ship)
{
	if(!selectedOutfit)
		return false;

	// if we're hovering above a button that can modify ship outfits, highlight the ship
	if(hoverButton == 'b')
		return CanBuy() && ShipCanAdd(ship, selectedOutfit);
	if(hoverButton == 'i')
		return CanInstall() && ShipCanAdd(ship, selectedOutfit);
	if(hoverButton == 's')
		return CanSell() && ShipCanRemove(ship, selectedOutfit);
	if(hoverButton == 'u')
		return CanUninstall() && ShipCanRemove(ship, selectedOutfit);

	return false;
}



// draw the display filter selection checkboxes, lower left of outfit panel
void OutfitterPanel::DrawKey()
{
	const Sprite *back = SpriteSet::Get("ui/outfitter key");
	SpriteShader::Draw(back, Screen::BottomLeft() + .5 * Point(back->Width(), -back->Height()));

	const Font &font = FontSet::Get(14);
	Color color[2] = {*GameData::Colors().Get("medium"), *GameData::Colors().Get("bright")};
	const Sprite *box[2] = {SpriteSet::Get("ui/unchecked"), SpriteSet::Get("ui/checked")};

	Point pos = Screen::BottomLeft() + Point(10., -VisibilityCheckboxesSize() + checkboxSpacing / 2.);
	const Point off = Point(10., -.5 * font.Height());
	const Point checkboxSize = Point(180., checkboxSpacing);
	const Point checkboxOffset = Point(80., 0.);

	SpriteShader::Draw(box[showForSale], pos);
	font.Draw("Show outfits for sale", pos + off, color[showForSale]);
	AddZone(Rectangle(pos + checkboxOffset, checkboxSize), [this](){ ToggleForSale(); });

	pos.Y() += checkboxSpacing;
	SpriteShader::Draw(box[showInstalled], pos);
	// text will be "medium" when no ships are selected, regardless of checkmark state
	font.Draw("Show installed outfits", pos + off, color[showInstalled && playerShip]);
	AddZone(Rectangle(pos + checkboxOffset, checkboxSize), [this]() { ToggleInstalled(); });

	pos.Y() += checkboxSpacing;
	SpriteShader::Draw(box[showCargo], pos);
	font.Draw("Show outfits in cargo", pos + off, color[showCargo]);
	AddZone(Rectangle(pos + checkboxOffset, checkboxSize), [this](){ ToggleCargo(); });

	pos.Y() += checkboxSpacing;
	SpriteShader::Draw(box[showStorage], pos);
	font.Draw("Show outfits in storage", pos + off, color[showStorage]);
	AddZone(Rectangle(pos + checkboxOffset, checkboxSize), [this](){ ToggleStorage(); });
}



void OutfitterPanel::ToggleForSale()
{
	showForSale = !showForSale;

	CheckSelection();
	delayedAutoScroll = true;
}



void OutfitterPanel::ToggleInstalled()
{
	showInstalled = !showInstalled;

	CheckSelection();
	delayedAutoScroll = true;
}



void OutfitterPanel::ToggleStorage()
{
	showStorage = !showStorage;

	CheckSelection();
	delayedAutoScroll = true;
}



void OutfitterPanel::ToggleCargo()
{
	showCargo = !showCargo;

	CheckSelection();
	delayedAutoScroll = true;
}



// Can this ship Install the selected Outfit
bool OutfitterPanel::ShipCanAdd(const Ship *ship, const Outfit *outfit)
{
	return (ship->Attributes().CanAdd(*outfit, 1) > 0);
}



// Can this ship Uninstall the selected Outfit
bool OutfitterPanel::ShipCanRemove(const Ship *ship, const Outfit *outfit)
{
	if(!ship->OutfitCount(outfit))
		return false;

	// If this outfit requires ammo, check if we could sell it if we sold all
	// the ammo for it first.
	const Outfit *ammo = outfit->Ammo();
	if(ammo && ship->OutfitCount(ammo))
	{
		Outfit attributes = ship->Attributes();
		attributes.Add(*ammo, -ship->OutfitCount(ammo));
		return attributes.CanAdd(*outfit, -1);
	}

	// Now, check whether this ship can uninstall this outfit.
	return ship->Attributes().CanAdd(*outfit, -1);
}



void OutfitterPanel::DrawOutfit(const Outfit &outfit, const Point &center, bool isSelected, bool isOwned)
{
	const Sprite *thumbnail = outfit.Thumbnail();
	const Sprite *back = SpriteSet::Get(
		isSelected ? "ui/outfitter selected" : "ui/outfitter unselected");
	SpriteShader::Draw(back, center);
	SpriteShader::Draw(thumbnail, center);

	// Draw the outfit name.
	const string &name = outfit.DisplayName();
	const Font &font = FontSet::Get(14);
	Point offset(-.5 * OUTFIT_SIZE, -.5 * OUTFIT_SIZE + 10.);
	font.Draw({name, {OUTFIT_SIZE, Alignment::CENTER, Truncate::MIDDLE}},
		center + offset, Color((isSelected | isOwned) ? .8 : .5, 0.));
}



bool OutfitterPanel::IsLicense(const string &name)
{
	static const string &LICENSE = " License";
	if(name.length() < LICENSE.length())
		return false;
	if(name.compare(name.length() - LICENSE.length(), LICENSE.length(), LICENSE))
		return false;

	return true;
}



bool OutfitterPanel::HasLicense(const string &name) const
{
	return (IsLicense(name) && player.HasLicense(LicenseRoot(name)));
}



string OutfitterPanel::LicenseRoot(const string &name)
{
	static const string &LICENSE = " License";
	return name.substr(0, name.length() - LICENSE.length());
}



void OutfitterPanel::CheckRefill()
{
	if(checkedRefill)
		return;
	checkedRefill = true;

	int count = 0;
	map<const Outfit *, int> needed;
	for(const shared_ptr<Ship> &ship : player.Ships())
	{
		// Skip ships in other systems and those that were unable to land in-system.
		if(ship->GetSystem() != player.GetSystem() || ship->IsDisabled())
			continue;

		++count;
		auto toRefill = GetRefillableAmmunition(*ship);
		for(const Outfit *outfit : toRefill)
		{
			int amount = ship->Attributes().CanAdd(*outfit, numeric_limits<int>::max());
			if(amount > 0)
			{
				bool available = outfitter.Has(outfit) || player.Stock(outfit) > 0;
				available = available || player.Cargo().Get(outfit) || player.Storage().Get(outfit);
				if(available)
					needed[outfit] += amount;
			}
		}
	}

	int64_t cost = 0;
	for(auto &it : needed)
	{
		// Don't count cost of anything installed from cargo or storage.
		it.second = max(0, it.second - player.Cargo().Get(it.first) - player.Storage().Get(it.first));
		if(!outfitter.Has(it.first))
			it.second = min(it.second, max(0, player.Stock(it.first)));
		cost += player.StockDepreciation().Value(it.first, day, it.second);
	}
	if(!needed.empty() && cost < player.Accounts().Credits())
	{
		string message = "Do you want to reload all the ammunition for your ship";
		message += (count == 1) ? "?" : "s?";
		if(cost)
			message += " It will cost " + Format::CreditString(cost) + ".";
		GetUI()->Push(new Dialog(this, &OutfitterPanel::Refill, message));
	}
}



void OutfitterPanel::Refill()
{
	for(const shared_ptr<Ship> &ship : player.Ships())
	{
		// Skip ships in other systems and those that were unable to land in-system.
		if(ship->GetSystem() != player.GetSystem() || ship->IsDisabled())
			continue;

		auto toRefill = GetRefillableAmmunition(*ship);
		for(const Outfit *outfit : toRefill)
		{
			int neededAmmo = ship->Attributes().CanAdd(*outfit, numeric_limits<int>::max());
			if(neededAmmo > 0)
			{
				// Fill first from any stockpiles in storage.
				const int fromStorage = player.Storage().Remove(outfit, neededAmmo);
				neededAmmo -= fromStorage;
				// Then from cargo.
				const int fromCargo = player.Cargo().Remove(outfit, neededAmmo);
				neededAmmo -= fromCargo;
				// Then, buy at reduced (or full) price.
				int available = outfitter.Has(outfit) ? neededAmmo : min<int>(neededAmmo, max<int>(0, player.Stock(outfit)));
				if(neededAmmo && available > 0)
				{
					int64_t price = player.StockDepreciation().Value(outfit, day, available);
					player.Accounts().AddCredits(-price);
					player.AddStock(outfit, -available);
				}
				ship->AddOutfit(outfit, available + fromStorage + fromCargo);
			}
		}
	}
}



// Determine which ships of the selected ships should be referenced in this
// iteration of Buy / Sell.
vector<Ship *> OutfitterPanel::GetShipsToOutfit(bool isBuy) const
{
	vector<Ship *> shipsToOutfit;
	int compareValue = isBuy ? numeric_limits<int>::max() : 0;
	int compareMod = 2 * isBuy - 1;
	for(Ship *ship : playerShips)
	{
		if((isBuy && !ShipCanAdd(ship, selectedOutfit))
				|| (!isBuy && !ShipCanRemove(ship, selectedOutfit)))
			continue;

		int count = ship->OutfitCount(selectedOutfit);
		if(compareMod * count < compareMod * compareValue)
		{
			shipsToOutfit.clear();
			compareValue = count;
		}
		if(count == compareValue)
			shipsToOutfit.push_back(ship);
	}

	return shipsToOutfit;
}



void OutfitterPanel::DrawButtons()
{
	// There will be two rows of buttons:
	//  [ Buy  ] [  Install  ] [ Cargo ]
	//  [ Sell ] [ Uninstall ] [ Keep  ] [ Leave ]
	// bottom to top
	const double rowTwoY = Screen::BottomRight().Y() - .5 * BUTTON_HEIGHT - 2. * BUTTON_ROW_PAD;
	const double rowOneY = rowTwoY - BUTTON_HEIGHT - BUTTON_ROW_PAD;
	// right to left
	const double buttonFourX = Screen::BottomRight().X() - .5 * BUTTON_4_WIDTH - 2. * BUTTON_COL_PAD;
	const double buttonThreeX = buttonFourX - (.5 * BUTTON_4_WIDTH + .5 * BUTTON_3_WIDTH) - BUTTON_COL_PAD;
	const double buttonTwoX = buttonThreeX - (.5 * BUTTON_3_WIDTH + .5 * BUTTON_2_WIDTH) - BUTTON_COL_PAD;
	const double buttonOneX = buttonTwoX - (.5 * BUTTON_2_WIDTH + .5 * BUTTON_1_WIDTH) - BUTTON_COL_PAD;

	// draw the button panel (shop side panel footer), same as standard plus one row of buttons
	const Point buttonPanelSize(SIDEBAR_WIDTH, ButtonPanelHeight());
	FillShader::Fill(Screen::BottomRight() - .5 * buttonPanelSize, buttonPanelSize,
		*GameData::Colors().Get("shop side panel background"));
	FillShader::Fill(
		Point(Screen::Right() - SIDEBAR_WIDTH / 2, Screen::Bottom() - ButtonPanelHeight()),
		Point(SIDEBAR_WIDTH, 1), *GameData::Colors().Get("shop side panel footer"));

	// Set up font size and colors for the credits
	const Font& font = FontSet::Get(14);
	const Color& bright = *GameData::Colors().Get("bright");
	const Color& dim = *GameData::Colors().Get("medium");
	const Color& back = *GameData::Colors().Get("panel background");

	// Draw the row for credits display
	const Point creditsPoint(
		Screen::Right() - SIDEBAR_WIDTH + 10,
		Screen::Bottom() - ButtonPanelHeight() + 5);
	font.Draw("You have:", creditsPoint, dim);
	const auto credits = Format::CreditString(player.Accounts().Credits());
	font.Draw({ credits, {SIDEBAR_WIDTH - 20, Alignment::RIGHT} }, creditsPoint, bright);

	// Draw the row for Fleet Cargo Space free
	const Point cargoPoint(
		Screen::Right() - SIDEBAR_WIDTH + 10,
		Screen::Bottom() - ButtonPanelHeight() + 25);
	font.Draw("Cargo Free:", cargoPoint, dim);
	string space = Format::Number(player.Cargo().Free()) + " / " + Format::Number(player.Cargo().Size());
	font.Draw({ space, {SIDEBAR_WIDTH - 20, Alignment::RIGHT} }, cargoPoint, bright);

	// Button Text colors
	const Font& bigFont = FontSet::Get(18);
	const Color& hover = *GameData::Colors().Get("hover");
	const Color& active = *GameData::Colors().Get("active");
	const Color& inactive = *GameData::Colors().Get("inactive");
	const Color* textColor = &inactive;
	const Point buttonOneSize = Point(BUTTON_1_WIDTH, BUTTON_HEIGHT);
	const Point buttonTwoSize = Point(BUTTON_2_WIDTH, BUTTON_HEIGHT);
	const Point buttonThreeSize = Point(BUTTON_3_WIDTH, BUTTON_HEIGHT);
	const Point buttonFourSize = Point(BUTTON_4_WIDTH, BUTTON_HEIGHT);

	// First row of buttons
	// ---------------------------------------------------------------------------
	static const string BUY = "_Buy";
	const Point buyCenter = Point(buttonOneX, rowOneY);
	FillShader::Fill(buyCenter, buttonOneSize, back);
    textColor = !CanBuy() ? &inactive : (hoverButton == 'b') ? &hover : &active;
	bigFont.Draw(BUY,
		buyCenter - .5 * Point(bigFont.Width(BUY), bigFont.Height()),
		*textColor);

	static const string INSTALL = "_Install";
	const Point installCenter = Point(buttonTwoX, rowOneY);
	FillShader::Fill(installCenter, buttonTwoSize, back);
    textColor = !CanInstall() ? &inactive : (hoverButton == 'i') ? &hover : &active;
	bigFont.Draw(INSTALL,
		installCenter - .5 * Point(bigFont.Width(INSTALL), bigFont.Height()),
		*textColor);

	static const string CARGO = "_Cargo";
	const Point cargoCenter = Point(buttonThreeX, rowOneY);
	FillShader::Fill(cargoCenter, buttonThreeSize, back);
    textColor = !(CanMoveToCargo() || CanBuyToCargo()) ? &inactive : (hoverButton == 'c') ? &hover : &active;
	bigFont.Draw(CARGO,
		cargoCenter - .5 * Point(bigFont.Width(CARGO), bigFont.Height()),
		*textColor);

	// Second row of buttons
	// ---------------------------------------------------------------------------
	static const string SELL = "_Sell";
	const Point sellCenter = Point(buttonOneX, rowTwoY);
	FillShader::Fill(sellCenter, buttonOneSize, back);
	textColor = !CanSell() ? &inactive : hoverButton == 's' ? &hover : &active;
	// The `Sell` text was too far right, hence the adjustment
	bigFont.Draw(SELL,
		sellCenter - .5 * Point(bigFont.Width(SELL) + 2, bigFont.Height()),
		*textColor);
	
	static const string UNINSTALL = "_Uninstall";
	const Point uninstallCenter = Point(buttonTwoX, rowTwoY);
	FillShader::Fill(uninstallCenter, buttonTwoSize, back);
    textColor = !CanUninstall() ? &inactive : (hoverButton == 'u') ? &hover : &active;
	bigFont.Draw(UNINSTALL,
		uninstallCenter - .5 * Point(bigFont.Width(UNINSTALL), bigFont.Height()),
		*textColor);

	static const string STORE = "Sto_re";
	const Point storageCenter = Point(buttonThreeX, rowTwoY);
	FillShader::Fill(storageCenter, buttonThreeSize, back);
    textColor = !CanMoveToStorage() ? &inactive : (hoverButton == 'r') ? &hover : &active;
	// The `Sto_re` text was too far right, hence the adjustment
	bigFont.Draw(STORE,
		storageCenter - .5 * Point(bigFont.Width(STORE) + 1, bigFont.Height()),
		*textColor);

	static const string LEAVE = "_Leave";
	const Point leaveCenter = Point(buttonFourX, rowTwoY);
	FillShader::Fill(leaveCenter, buttonFourSize, back);
	bigFont.Draw(LEAVE,
		leaveCenter - .5 * Point(bigFont.Width(LEAVE), bigFont.Height()),
		hoverButton == 'l' ? hover : active);
	// ---------------------------------------------------------------------------

	// Find button
	const Point findCenter = Screen::BottomRight() - Point(580, 20);
	const Sprite* findIcon =
		hoverButton == 'f' ? SpriteSet::Get("ui/find selected") : SpriteSet::Get("ui/find unselected");
	SpriteShader::Draw(findIcon, findCenter);
	static const string FIND = "_Find";

	// Modifier Text that appears below Buy & Sell
	int modifier = Modifier();
	if (modifier > 1)
	{
		string mod = "x " + to_string(modifier);
		int modWidth = font.Width(mod);
		font.Draw(mod, buyCenter + Point(-.5 * modWidth, 10.), dim);
		font.Draw(mod, sellCenter + Point(-.5 * modWidth, 10.), dim);
		font.Draw(mod, installCenter + Point(-.5 * modWidth, 10.), dim);
		font.Draw(mod, uninstallCenter + Point(-.5 * modWidth, 10.), dim);
		font.Draw(mod, cargoCenter + Point(-.5 * modWidth, 10.), dim);
		font.Draw(mod, storageCenter + Point(-.5 * modWidth, 10.), dim);
	}

	// Draw the tooltip for your full number of credits.
	const Rectangle creditsBox = Rectangle::FromCorner(creditsPoint, Point(SIDEBAR_WIDTH - 20, 15));
	if (creditsBox.Contains(ShopPanel::hoverPoint))
		ShopPanel::hoverCount += ShopPanel::hoverCount < ShopPanel::HOVER_TIME;
	else if (ShopPanel::hoverCount)
		--ShopPanel::hoverCount;

	if (ShopPanel::hoverCount == ShopPanel::HOVER_TIME)
	{
		string text = Format::Number(player.Accounts().Credits()) + " credits";
		ShopPanel::DrawTooltip(text, hoverPoint, dim, *GameData::Colors().Get("tooltip background"));
	}
}



// Check if the given point is within the button zone, and if so return the
// letter of the button (or ' ' if it's not on a button).
char OutfitterPanel::CheckButton(int x, int y)
{
	// The Find button
	if (x > Screen::Right() - SIDEBAR_WIDTH - 342 && x < Screen::Right() - SIDEBAR_WIDTH - 316 &&
		y > Screen::Bottom() - 31 && y < Screen::Bottom() - 4)
		return 'f';

	if (x < Screen::Right() - SIDEBAR_WIDTH || y < Screen::Bottom() - ButtonPanelHeight())
		return '\0';

	// Check if not in a row of Buttons
	// bottom to top
	const double rowTwoTop = Screen::Bottom() - BUTTON_HEIGHT - 2. * BUTTON_ROW_PAD;
	const double rowOneTop = rowTwoTop - BUTTON_HEIGHT - BUTTON_ROW_PAD;
	// right to left
	const double buttonFourLeft = Screen::Right() - BUTTON_4_WIDTH - 2. * BUTTON_COL_PAD;
	const double buttonThreeLeft = buttonFourLeft - (.5 * BUTTON_4_WIDTH + .5 * BUTTON_3_WIDTH) - BUTTON_COL_PAD;
	const double buttonTwoLeft = buttonThreeLeft - (.5 * BUTTON_3_WIDTH + .5 * BUTTON_2_WIDTH) - BUTTON_COL_PAD;
	const double buttonOneLeft = buttonTwoLeft - (.5 * BUTTON_2_WIDTH + .5 * BUTTON_1_WIDTH) - BUTTON_COL_PAD;

	if (rowOneTop < y && y <= rowOneTop + BUTTON_HEIGHT)
	{
		if (buttonOneLeft <= x && x < buttonOneLeft + BUTTON_1_WIDTH)
			return 'b'; // BUY
		if (buttonTwoLeft <= x && x < buttonTwoLeft + BUTTON_2_WIDTH)
			return 'i'; // INSTALL
		if (buttonThreeLeft <= x && x < buttonThreeLeft + BUTTON_3_WIDTH)
			return 'c'; // CARGO
	}

	if (rowTwoTop < y && y <= rowTwoTop + BUTTON_HEIGHT)
	{
		if (buttonOneLeft <= x && x < buttonOneLeft + BUTTON_1_WIDTH)
			return 's'; // SELL
		if (buttonTwoLeft <= x && x < buttonTwoLeft + BUTTON_2_WIDTH)
			return 'u'; // UNINSTALL
		if (buttonThreeLeft <= x && x < buttonThreeLeft + BUTTON_3_WIDTH)
			return 'r'; // STORE
		if (buttonFourLeft <= x && x < buttonFourLeft + BUTTON_4_WIDTH)
			return 'l'; // LEAVE
	}

	return ' ';
}



int OutfitterPanel::FindItem(const string &text) const
{
	int bestIndex = 9999;
	int bestItem = -1;
	auto it = zones.begin();
	for(unsigned int i = 0; i < zones.size(); ++i, ++it)
	{
		const Outfit *outfit = it->GetOutfit();
		int index = Format::Search(outfit->DisplayName(), text);
		if(index >= 0 && index < bestIndex)
		{
			bestIndex = index;
			bestItem = i;
			if(!index)
				return i;
		}
	}
	return bestItem;
}
