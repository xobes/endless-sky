/* MenuPanel.cpp
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

#include "MenuPanel.h"

#include "audio/Audio.h"
#include "Command.h"
#include "Files.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "text/Format.h"
#include "GameData.h"
#include "Information.h"
#include "Interface.h"
#include "LoadPanel.h"
#include "Logger.h"
#include "MainPanel.h"
#include "pi.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Point.h"
#include "PreferencesPanel.h"
#include "Ship.h"
#include "image/Sprite.h"
#include "shader/StarField.h"
#include "StartConditionsPanel.h"
#include "System.h"
#include "text/TrueTypeFont.h"
#include "UI.h"

#include "opengl.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <format>

#include "shader/FillShader.h"
#include "Screen.h"
#include "shader/RingShader.h"
#include "text/DisplayText.h"

using namespace std;

namespace {
	const int SCROLL_MOD = 2;
	int scrollSpeed = 1;
	bool showCreditsWarning = true;
}



MenuPanel::MenuPanel(PlayerInfo &player, UI &gamePanels)
	: player(player), gamePanels(gamePanels), mainMenuUi(GameData::Interfaces().Get("main menu"))
{
	assert(GameData::IsLoaded() && "MenuPanel should only be created after all data is fully loaded");
	SetIsFullScreen(true);

	if(mainMenuUi->GetBox("credits").Dimensions())
	{
		for(const auto &source : GameData::Sources())
		{
			auto credit = Format::Split(Files::Read(source / "credits.txt"), "\n");
			if((credit.size() > 1) || !credit.front().empty())
			{
				credits.insert(credits.end(), credit.begin(), credit.end());
				credits.insert(credits.end(), 15, "");
			}
		}
		// Remove the last 15 lines, as there is already a gap at the beginning of the credits.
		credits.resize(credits.size() - 15);
	}
	else if(showCreditsWarning)
	{
		Logger::LogError("Warning: interface \"main menu\" does not contain a box for \"credits\"");
		showCreditsWarning = false;
	}

	if(gamePanels.IsEmpty())
	{
		gamePanels.Push(new MainPanel(player));
		// It takes one step to figure out the planet panel should be created, and
		// another step to actually place it. So, take two steps to avoid a flicker.
		gamePanels.StepAll();
		gamePanels.StepAll();
	}

	if(!scrollSpeed)
		scrollSpeed = 1;

	xSpeed = mainMenuUi->GetValue("x speed");
	ySpeed = mainMenuUi->GetValue("y speed");
	yAmplitude = mainMenuUi->GetValue("y amplitude");
	returnPos = GameData::GetBackgroundPosition();
	GameData::SetBackgroundPosition(Point());

	// When the player is in the menu, pause the game sounds.
	Audio::Pause();
}



MenuPanel::~MenuPanel()
{
	Audio::Resume();
	GameData::SetBackgroundPosition(returnPos);
}



void MenuPanel::Step()
{
	if(Preferences::Has("Animate main menu background"))
	{
		GameData::StepBackground(Point(xSpeed, yAmplitude * sin(animation * TO_RAD)));
		animation += ySpeed;
	}
	else
		GameData::StepBackground(Point());
	if(GetUI()->IsTop(this) && !scrollingPaused)
	{
		scroll += scrollSpeed;
		if(scroll < 0)
			scroll = (20 * static_cast<long long int>(credits.size()) + 299) * SCROLL_MOD;
		if(scroll >= (20 * static_cast<long long int>(credits.size()) + 300) * SCROLL_MOD)
			scroll = 0;
	}
}



void MenuPanel::Draw()
{
	glClear(GL_COLOR_BUFFER_BIT);
	GameData::Background().Draw(Point());

	Information info;
	if(player.IsLoaded() && !player.IsDead())
	{
		info.SetCondition("pilot loaded");
		info.SetString("pilot", player.FirstName() + " " + player.LastName());
		if(player.Flagship())
		{
			const Ship &flagship = *player.Flagship();
			info.SetSprite("ship sprite", flagship.GetSprite());
			info.SetString("ship", flagship.Name());
		}
		if(player.GetSystem())
			info.SetString("system", player.GetSystem()->DisplayName());
		if(player.GetPlanet())
			info.SetString("planet", player.GetPlanet()->DisplayName());
		info.SetString("credits", Format::Credits(player.Accounts().Credits()));
		info.SetString("date", player.GetDate().ToString());
		info.SetString("playtime", Format::PlayTime(player.GetPlayTime()));
	}
	else if(player.IsLoaded())
	{
		info.SetCondition("no pilot loaded");
		info.SetString("pilot", player.FirstName() + " " + player.LastName());
		info.SetString("ship", "You have died.");
	}
	else
	{
		info.SetCondition("no pilot loaded");
		info.SetString("pilot", "No Pilot Loaded");
	}

	GameData::Interfaces().Get("menu background")->Draw(info, this);
	mainMenuUi->Draw(info, this);
	GameData::Interfaces().Get("menu player info")->Draw(info, this);

	if(!credits.empty())
		DrawCredits();
}



bool MenuPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	if(player.IsLoaded() && (key == 'e' || command.Has(Command::MENU)))
	{
		gamePanels.CanSave(true);
		GetUI()->PopThrough(this);
		return true;
	}
	else if(key == 'p')
		GetUI()->Push(new PreferencesPanel(player));
	else if(key == 'l' || key == 'm')
		GetUI()->Push(new LoadPanel(player, gamePanels));
	else if(key == 'n' && (!player.IsLoaded() || player.IsDead()))
	{
		// If no player is loaded, the "Enter Ship" button becomes "New Pilot."
		// Request that the player chooses a start scenario.
		// StartConditionsPanel also handles the case where there's no scenarios.
		GetUI()->Push(new StartConditionsPanel(player, gamePanels, GameData::StartOptions(), nullptr));
	}
	else if(key == 'q')
	{
		GetUI()->Quit();
		return true;
	}
	else if(key == ' ')
		scrollingPaused = !scrollingPaused;
	else if(key == SDLK_DOWN)
		scrollSpeed += 1;
	else if(key == SDLK_UP)
		scrollSpeed -= 1;
	else
		return false;

	UI::PlaySound(UI::UISound::NORMAL);
	return true;
}



bool MenuPanel::Click(int x, int y, MouseButton button, int clicks)
{
	if(button != MouseButton::LEFT)
		return false;

	// Double clicking on the credits pauses/resumes the credits scroll.
	if(clicks == 2 && mainMenuUi->GetBox("credits").Contains(Point(x, y)))
	{
		scrollingPaused = !scrollingPaused;
		return true;
	}

	return false;
}



void MenuPanel::DrawCredits() const
{
	const Font &font = FontSet::Get(14);
	const TrueTypeFont &tfont = FontSet::GetTTF(14);

	const auto creditsRect = mainMenuUi->GetBox("credits");
	const int top = static_cast<int>(creditsRect.Top());
	const int bottom = static_cast<int>(creditsRect.Bottom());
	int y = bottom + 5 - scroll / SCROLL_MOD;
	for(const string &line : credits)
	{
		float fade = 1.f;
		if(y < top + 20)
			fade = max(0.f, (y - top) / 20.f);
		else if(y > bottom - 20)
			fade = max(0.f, (bottom - y) / 20.f);
		if(fade)
		{
			Color color(((line.empty() || line[0] == ' ') ? .2f : .4f) * fade, 0.f);
			font.Draw(line, Point(creditsRect.Left(), y), color);
			Color black(0);
			tfont.Draw(line, Point(creditsRect.Left(), y), black);
		}
		y += 20;
	}
	DrawDebug();
}



void MenuPanel::DrawDebug() const
{
	const Font &font = FontSet::Get(14);
	const TrueTypeFont &tfont = FontSet::GetTTF(14);

	// TODO: now try with colors
	// - all the defined colors
	// - rgb variations
	// - was it alpha all along?
	Color colors[] = {
		{0.5, 0.5, 0.5, 0},
		{1., 1., 1., 0.},
		{.75, .75, .75, 0.},
		{.5, .5, .5, 0.},
		{.25, .25, .25, 0.},
		{.18, .18, .18, 0.},
		{.1, .1, .1, 0.},
		{.05, .05, .05, 0.},
		{.5, .5, .1, 0.},
		{.5, .3, .1, 0.},
		{.4, 0., 0., 0.},
		{.125, .125, .125, 1.},
		{.3, .3, .3, .0},
		{.2, .2, .2, 1.},
		{.055, .055, .055, 1.},
		{.5, .375, 0., 1.},
		{.8, .6, 0., 1.},
		{0., .375, .5, 1.},
		{0., .6, .8, 1.},
		{.09, .09, .09, 1.},
		{1., .5, 0., 1.},
		{.0937, .0937, .0937, 1.},
		{.43, .55, .85, .8},
		{.70, .62, .43, .75},
		{.3, 0, 0, .3},
		{.70, .43, .43, .75},
		{.70, .61, .43, .75},
		{.6, .6, .6, .75},
		{.70, .62, .43, .75},
		{.43, .55, .70, 0.},
		{.70, .62, .43, 0.},
		{.70, .43, .43, 0.},
		{.6, .6, .6, 0.},
		{.630, .62, .43, 0.},
		{.215, .275, .35, 0.},
		{.35, .31, .215, 0.},
		{.35, .215, .215, 0.},
		{.35, .31, .215, 0.},
		{.5, .8, .2, 0.},
		{.45, 0., 0., .65},
		{.2, 1., 0., 0.},
		{.9, .6, 0., 1.},
		{.5, .3, 0., .5},
		{.5, .3, 0., 1.},
		{.3, .1, 0., .5},
		{.2, .7, 1., 1.},
		{0., .4, .5, .5},
		{0., .4, .6, 1.},
		{0., .15, .2, .5},
		{.1, .2, .9, 1.},
		{0., .3, .7, .5},
		{.2, .1, 0., 0.},
		{.4, .4, .4, 0.},
		{.8, .8, .8, 1.},
		{.4, .4, .6, 1.},
		{.9, .8, 0., 1.},
		{.9, .2, 0., 1.},
		{.2, .8, 0., 1.},
		{1., .6, .4, 1.},
		{0., .5, 0., .25},
		{.45, .5, 0., .25},
		{.5, 0., 0., .25},
		{.5, .15, 0., .25},
		{.5, .3, 0., .25},
		{.3, .3, 0., .25},
		{.43, .55, .70, .75},
		{.45, .5, 0., .25},
		{.5, 0., 0., .25},
		{.25, .25, .25, .25},
		{.7, .7, .7, .25},
		{.35, .35, .35, .25},
		{1., 1., .25, .7},
		{1., .9, 0., .9},
		{1., .1, .1, 1.},
		{0., 1., 0., .1},
		{1., 0., 0., .5},
		{.2, 1., 0., 0.},
		{.4, .6, 1., 0.},
		{.8, .8, .3, 0.},
		{.85, .3, .2, 0.},
		{.7, 0., 1., 0.},
		{0., .3, 0., 0.},
		{.2, 1., 0., 0.},
		{.4, .6, 1., 0.},
		{.8, .8, .3, 0.},
		{.85, .3, .2, 0.},
		{.2, 1., 0., 0.},
		{.4, .6, 1., 0.},
		{.8, .8, .3, 0.},
		{.85, .3, .2, 0.},
		{.4, .6, 1., 0.},
		{.8, .8, .3, 0.},
		{.85, .3, .2, 0.},
		{.25, .1, .1, 1.},
		{.21, .18, .08, 1.},
		{.315, .27, .12, 1.},
		{.16, .16 , .13, 1.},
		{1., .5, 0., 1.},
		{0., 1., .5, 1.},
		{1., .5, 0., 1.},
		{0., 1., .5, 1.},
		{0., .5, 1., 1.},
		{.6, .6, .6, .6},
		{.2, .5, 0., 0.},
		{.5, .4, 0., 0.},
		{.55, .1, 0., 0.},
		{.5, .2, .9, 1.},
		{.1, .6, 0., .4},
		{.4, .4, .4, .6},
		{1., 0., 0., 1.},
		{0., 0., 0., 1.},
		{0, .7, .1, .3},
		{.19, .27, .69, 0},
		{.75, .1, .1, 0.},
	};

	double height = 40.;
	double width = 50.;
	int m = 10; // x, width
	int n = 10; // y, height
	Point p(Screen::Left() + 40, Screen::Top() + 40);
	for(int i = 0; i < m; i++)
	{
		for(int j = 0; j < n; j++)
		{
			Color b = colors[i * m + j];
			font.Draw("pilot:", p + Point(0, 20), b);
			tfont.Draw({"pilot:", {Alignment::LEFT}}, p + Point(0, 0), b);
			p.Y() += height;
		}
		p.Y() = Screen::Top() + 40;
		p.X() += width;
	}
}
