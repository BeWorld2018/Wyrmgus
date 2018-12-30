//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name site.cpp - The site source file. */
//
//      (c) Copyright 2018 by Andrettin
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include "stratagus.h"

#include "map/site.h"

#include "civilization.h"
#include "config.h"
#include "map/map_template.h"
#include "province.h" // for regions

/*----------------------------------------------------------------------------
--  Variables
----------------------------------------------------------------------------*/

std::vector<CSite *> CSite::Sites;
std::map<std::string, CSite *> CSite::SitesByIdent;

/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

/**
**	@brief	Get a site
**
**	@param	ident			The site's string identifier
**	@param	should_find		Whether it is an error if the site could not be found; this is true by default
**
**	@return	The site if found, or null otherwise
*/
CSite *CSite::GetSite(const std::string &ident)
{
	if (ident.empty()) {
		return nullptr;
	}
	
	std::map<std::string, CSite *>::const_iterator find_iterator = SitesByIdent.find(ident);
	
	if (find_iterator != SitesByIdent.end()) {
		return find_iterator->second;
	}
	
	return nullptr;
}

/**
**	@brief	Get or add a site
**
**	@param	ident	The site's string identifier
**
**	@return	The site if found, or a newly-created one otherwise
*/
CSite *CSite::GetOrAddSite(const std::string &ident)
{
	CSite *site = GetSite(ident);
	
	if (!site) {
		site = new CSite;
		site->Ident = ident;
		Sites.push_back(site);
		SitesByIdent[ident] = site;
	}
	
	return site;
}

/**
**	@brief	Remove the existing sites
*/
void CSite::ClearSites()
{
	for (CSite *site : Sites) {
		delete site;
	}
	Sites.clear();
}

/**
**	@brief	Process data provided by a configuration file
**
**	@param	config_data	The configuration data
*/
void CSite::ProcessConfigData(const CConfigData *config_data)
{
	for (size_t i = 0; i < config_data->Properties.size(); ++i) {
		std::string key = config_data->Properties[i].first;
		std::string value = config_data->Properties[i].second;
		
		if (key == "name") {
			this->Name = value;
		} else if (key == "major") {
			this->Major = StringToBool(value);
		} else if (key == "position_x") {
			this->Position.x = std::stoi(value);
		} else if (key == "position_y") {
			this->Position.y = std::stoi(value);
		} else if (key == "map_template") {
			value = FindAndReplaceString(value, "_", "-");
			this->MapTemplate = CMapTemplate::GetMapTemplate(value);
		} else if (key == "region") {
			value = FindAndReplaceString(value, "_", "-");
			
			CRegion *region = GetRegion(value);
			if (region != nullptr) {
				this->Regions.push_back(region);
				region->Sites.push_back(this);
			} else {
				fprintf(stderr, "Invalid region: \"%s\".\n", value.c_str());
			}
		} else {
			fprintf(stderr, "Invalid site property: \"%s\".\n", key.c_str());
		}
	}
	
	for (const CConfigData *child_config_data : config_data->Children) {
		if (child_config_data->Tag == "cultural_names") {
			for (size_t j = 0; j < child_config_data->Properties.size(); ++j) {
				std::string key = child_config_data->Properties[j].first;
				std::string value = child_config_data->Properties[j].second;
				
				key = FindAndReplaceString(key, "_", "-");
				
				const CCivilization *civilization = CCivilization::GetCivilization(key);
				
				if (civilization) {
					this->CulturalNames[civilization] = value;
				}
			}
		} else {
			fprintf(stderr, "Invalid site property: \"%s\".\n", child_config_data->Tag.c_str());
		}
	}
	
	if (!this->Major && !this->Cores.empty()) { //if the site is a minor one, but has faction cores, remove them
		for (size_t i = 0; i < this->Cores.size(); ++i) {
			CFaction *core_faction = this->Cores[i];
			core_faction->Cores.erase(std::remove(core_faction->Cores.begin(), core_faction->Cores.end(), this), core_faction->Cores.end());
		}
		this->Cores.clear();
	}
	
	if (this->MapTemplate) {
		if (this->Position.x != -1 && this->Position.y != -1) {
			if (this->MapTemplate->SitesByPosition.find(std::pair<int, int>(this->Position.x, this->Position.y)) == this->MapTemplate->SitesByPosition.end()) {
				this->MapTemplate->SitesByPosition[std::pair<int, int>(this->Position.x, this->Position.y)] = this;
			} else {
				fprintf(stderr, "Position (%d, %d) of map template \"%s\" already has a site.", this->Position.x, this->Position.y, this->MapTemplate->Ident.c_str());
			}
		}
		
		this->MapTemplate->Sites.push_back(this);
	}
}

/**
**	@brief	Get a site's cultural name
**
**	@param	civilization	The civilization for which to get the cultural name
**
**	@return	The cultural name if present, or the default name otherwise
*/
/**
**  
*/
std::string CSite::GetCulturalName(const CCivilization *civilization) const
{
	if (civilization != nullptr) {
		std::map<const CCivilization *, std::string>::const_iterator find_iterator = this->CulturalNames.find(civilization);
		if (find_iterator != this->CulturalNames.end()) {
			return find_iterator->second;
		}
	}
	
	return this->Name;
}