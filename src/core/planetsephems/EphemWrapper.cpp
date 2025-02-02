/*
Copyright (C) 2003 Fabien Chereau
Copyright (C) 2015 Holger Niessner
Copyright (C) 2015 Georg Zotti

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
*/

#include "EphemWrapper.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "vsop87.h"
#include "elp82b.h"
#include "marssat.h"
#include "l1.h"
#include "tass17.h"
#include "gust86.h"
#include "de431.hpp"
#include "de430.hpp"
#include "pluto.h"

#define EPHEM_MERCURY_ID  0
#define EPHEM_VENUS_ID    1
#define EPHEM_EMB_ID    2
#define EPHEM_MARS_ID   3
#define EPHEM_JUPITER_ID  4
#define EPHEM_SATURN_ID   5
#define EPHEM_URANUS_ID   6
#define EPHEM_NEPTUNE_ID  7
#define EPHEM_PLUTO_ID    8

// GZ No idea what IMD stands for?
//#define EPHEM_IMD_EARTH_ID 2
#define EPHEM_JPL_EARTH_ID 3
#define EPHEM_JPL_PLUTO_ID 9
#define EPHEM_JPL_MOON_ID 10
#define EPHEM_JPL_SUN_ID 11

/**   JPL PLANET ID LIST
**            1 = mercury           8 = neptune                             **
**            2 = venus             9 = pluto                               **
**            3 = earth            10 = moon                                **
**            4 = mars             11 = sun                                 **
**            5 = jupiter          12 = solar-system barycenter             **
**            6 = saturn           13 = earth-moon barycenter               **
**            7 = uranus 
**/

void EphemWrapper::init_de430(const char* filepath)
{
  InitDE430(filepath);
}

void EphemWrapper::init_de431(const char* filepath)
{
  InitDE431(filepath);
}

bool jd_fits_de431(const double jd)
{
	//Correct limits found via jpl_get_double(). Limits hardcoded to avoid calls each time.
	//return !(jd < -3027215.5 || jd > 7930192.5);
	//This limits inside those where sun can jump between ecliptic of date and ecliptic2000.
	// We lose a month in -13000 and a few months in +17000, this should not matter.
	return ((jd > -3027188.25 ) && (jd < 7930056.87916));
}

bool jd_fits_de430(const double jd)
{
	//return !(jd < 2287184.5 || jd > 2688974.5);
	//return !(jd < 2287184.5 || jd > 2688976.5);
	return ((jd > 2287184.5) && (jd < 2688976.5));
}

bool use_de430(const double jd)
{
    return StelApp::getInstance().getCore()->de430IsActive() &&
        jd_fits_de430(jd);
}

bool use_de431(const double jd)
{
    return StelApp::getInstance().getCore()->de431IsActive() &&
        jd_fits_de431(jd);
}

// planet_id is ONLY one of the #defined values 0..8 above.
void get_planet_helio_coordsv(const double jd, double xyz[3], const int planet_id)
{
	bool deOk=false;
	if(!std::isfinite(jd))
	{
		qDebug() << "get_planet_helio_coordsv(): SKIPPED CoordCalc, jd is infinite/nan: " << jd;
		return;
	}

	if(use_de430(jd))
	{
		deOk=GetDe430Coor(jd, planet_id + 1, xyz);
	}
	else if(use_de431(jd))
	{
		deOk=GetDe431Coor(jd, planet_id + 1, xyz);
	}
	if (!deOk) //VSOP87 as fallback
	{
		GetVsop87Coor(jd, planet_id, xyz);
	}
}

// Osculating positions for time JDE in elements for JDE0, if possible by the theory used (e.g. VSOP87).
// For ephemerides like DE4xx, JDE0 is irrelevant.
void get_planet_helio_osculating_coordsv(double jd0, double jd, double xyz[3], int planet_id)
{
	bool deOk=false;
	if(!(std::isfinite(jd) && std::isfinite(jd0)))
	{
		qDebug() << "get_planet_helio_osculating_coordsv(): SKIPPED CoordCalc, jd0 or jd is infinite/nan. jd0:" << jd0 << "jd: "<< jd;
		return;
	}

	if(use_de430(jd))
	{
		deOk=GetDe430Coor(jd, planet_id + 1, xyz);
	}
	else if(use_de431(jd))
	{
		deOk=GetDe431Coor(jd, planet_id + 1, xyz);
	}
	if (!deOk) //VSOP87 as fallback
	{
		GetVsop87OsculatingCoor(jd0, jd, planet_id, xyz);
	}
}

/* Chapter 31 Pg 206-207 Equ 31.1 31.2, 31.3 using VSOP 87
 * Calculate Pluto's rectangular heliocentric ecliptical coordinates
 * for given Julian Day. Values are in AU.
 * params : Julian day, rect coords */

void get_pluto_helio_coordsv(double jd,double xyz[3], void* unused)
{
	Q_UNUSED(unused);
	bool deOk=false;
	if(!std::isfinite(jd))
	{
		qDebug() << "get_pluto_helio_coordsv(): SKIPPED PlutoCoordCalc, jd is infinite/nan:" << jd;
		return;
	}

	if(use_de430(jd))
	{
		deOk=GetDe430Coor(jd, EPHEM_JPL_PLUTO_ID, xyz);
	}
	else if(use_de431(jd))
	{
		deOk=GetDe431Coor(jd, EPHEM_JPL_PLUTO_ID, xyz);
	}
	if (!deOk) // fallback to previous solution
	{
		get_pluto_helio_coords(jd, &xyz[0], &xyz[1], &xyz[2]);
	}
}

/* Return 0 for the sun */
void get_sun_helio_coordsv(double jd,double xyz[3], void* unused)
{
    xyz[0]=0.; xyz[1]=0.; xyz[2]=0.;
}

void get_mercury_helio_coordsv(double jd,double xyz[3], void* unused)
{
  	get_planet_helio_coordsv(jd, xyz, EPHEM_MERCURY_ID);
}
void get_venus_helio_coordsv(double jd,double xyz[3], void* unused)
{
  	get_planet_helio_coordsv(jd, xyz, EPHEM_VENUS_ID);
}

void get_earth_helio_coordsv(const double jd,double xyz[3], void* unused) 
{
	bool deOk=false;
	if(!std::isfinite(jd))
	{
		qDebug() << "get_earth_helio_coordsv(): SKIPPED EarthCoordCalc, jd is infinite/nan:" << jd;
		return;
	}

	if(use_de430(jd))
	{
		deOk=GetDe430Coor(jd, EPHEM_JPL_EARTH_ID, xyz);
	}
	else if(use_de431(jd))
	{
		deOk=GetDe431Coor(jd, EPHEM_JPL_EARTH_ID, xyz);
	}
	if (!deOk) //VSOP87 as fallback
	{
		double moon[3];
		GetVsop87Coor(jd,EPHEM_EMB_ID,xyz);
		GetElp82bCoor(jd,moon);
		/* Earth != EMB:
	0.0121505677733761 = mu_m/(1+mu_m),
	mu_m = mass(moon)/mass(earth) = 0.01230002 */
		xyz[0] -= 0.0121505677733761 * moon[0];
		xyz[1] -= 0.0121505677733761 * moon[1];
		xyz[2] -= 0.0121505677733761 * moon[2];
	}
}

void get_mars_helio_coordsv(double jd,double xyz[3], void* unused)
{
	 get_planet_helio_coordsv(jd, xyz, EPHEM_MARS_ID);
}

void get_jupiter_helio_coordsv(double jd,double xyz[3], void* unused)
{
	 get_planet_helio_coordsv(jd, xyz, EPHEM_JUPITER_ID);
}

void get_saturn_helio_coordsv(double jd,double xyz[3], void* unused)
{
	 get_planet_helio_coordsv(jd, xyz, EPHEM_SATURN_ID);
}

void get_uranus_helio_coordsv(double jd,double xyz[3], void* unused)
{
	 get_planet_helio_coordsv(jd, xyz, EPHEM_URANUS_ID);
}

void get_neptune_helio_coordsv(double jd,double xyz[3], void* unused)
{
	 get_planet_helio_coordsv(jd, xyz, EPHEM_NEPTUNE_ID);
}

void get_mercury_helio_osculating_coords(double jd0,double jd,double xyz[3])
{
	 get_planet_helio_osculating_coordsv(jd0, jd, xyz, EPHEM_MERCURY_ID);
}

void get_venus_helio_osculating_coords(double jd0,double jd,double xyz[3])
{
	 get_planet_helio_osculating_coordsv(jd0, jd, xyz, EPHEM_VENUS_ID);
}

void get_earth_helio_osculating_coords(double jd0,double jd,double xyz[3])
{
	 get_planet_helio_osculating_coordsv(jd0, jd, xyz, EPHEM_EMB_ID);
}

void get_mars_helio_osculating_coords(double jd0,double jd,double xyz[3])
{
	 get_planet_helio_osculating_coordsv(jd0, jd, xyz, EPHEM_MARS_ID);
}

void get_jupiter_helio_osculating_coords(double jd0,double jd,double xyz[3])
{
	 get_planet_helio_osculating_coordsv(jd0, jd, xyz, EPHEM_JUPITER_ID);
}

void get_saturn_helio_osculating_coords(double jd0,double jd,double xyz[3])
{
	 get_planet_helio_osculating_coordsv(jd0, jd, xyz, EPHEM_SATURN_ID);
}

void get_uranus_helio_osculating_coords(double jd0,double jd,double xyz[3])
{
	 get_planet_helio_osculating_coordsv(jd0, jd, xyz, EPHEM_URANUS_ID);
}

void get_neptune_helio_osculating_coords(double jd0,double jd,double xyz[3])
{
	 get_planet_helio_osculating_coordsv(jd0, jd, xyz, EPHEM_NEPTUNE_ID);
}

/* Calculate the rectangular geocentric lunar coordinates to the inertial mean
 * ecliptic and equinox of J2000.
 * The geocentric coordinates returned are in units of AU.
 * This function is based upon the Lunar Solution ELP2000-82B by
 * Michelle Chapront-Touze and Jean Chapront of the Bureau des Longitudes,
 * Paris. ELP 2000-82B theory
 * param jd Julian day, rect pos */
void get_lunar_parent_coordsv(double jde,double xyz[3], void* unused)
{
	bool deOk=false;
	if(use_de430(jde))
		deOk=GetDe430Coor(jde, EPHEM_JPL_MOON_ID, xyz, EPHEM_JPL_EARTH_ID);
	else if(use_de431(jde))
		deOk=GetDe431Coor(jde, EPHEM_JPL_MOON_ID, xyz, EPHEM_JPL_EARTH_ID);
	if (!deOk) // fallback...
		GetElp82bCoor(jde,xyz);
}

void get_phobos_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetMarsSatCoor(jd,MARS_SAT_PHOBOS,xyz);
}

void get_deimos_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetMarsSatCoor(jd,MARS_SAT_DEIMOS,xyz);
}

void get_io_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetL1Coor(jd,L1_IO,xyz);
}

void get_europa_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetL1Coor(jd,L1_EUROPA,xyz);
}

void get_ganymede_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetL1Coor(jd,L1_GANYMEDE,xyz);
}

void get_callisto_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetL1Coor(jd,L1_CALLISTO,xyz);
}

void get_mimas_parent_coordsv(double jd,double xyz[3], void* unused)
{ 
    GetTass17Coor(jd,TASS17_MIMAS,xyz);
}

void get_enceladus_parent_coordsv(double jd,double xyz[3], void* unused)
{ 
    GetTass17Coor(jd,TASS17_ENCELADUS,xyz);
}

void get_tethys_parent_coordsv(double jd,double xyz[3], void* unused)
{ 
    GetTass17Coor(jd,TASS17_TETHYS,xyz);
}

void get_dione_parent_coordsv(double jd,double xyz[3], void* unused)
{ 
    GetTass17Coor(jd,TASS17_DIONE,xyz);
}

void get_rhea_parent_coordsv(double jd,double xyz[3], void* unused)
{ 
    GetTass17Coor(jd,TASS17_RHEA,xyz);
}

void get_titan_parent_coordsv(double jd,double xyz[3], void* unused)
{ 
    GetTass17Coor(jd,TASS17_TITAN,xyz);
}

void get_hyperion_parent_coordsv(double jd,double xyz[3], void* unused)
{ 
    GetTass17Coor(jd,TASS17_HYPERION,xyz);
}

void get_iapetus_parent_coordsv(double jd,double xyz[3], void* unused)
{ 
    GetTass17Coor(jd,TASS17_IAPETUS,xyz);
}

void get_miranda_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetGust86Coor(jd,GUST86_MIRANDA,xyz);
}

void get_ariel_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetGust86Coor(jd,GUST86_ARIEL,xyz);
}

void get_umbriel_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetGust86Coor(jd,GUST86_UMBRIEL,xyz);
}

void get_titania_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetGust86Coor(jd,GUST86_TITANIA,xyz);
}

void get_oberon_parent_coordsv(double jd,double xyz[3], void* unused)
{
    GetGust86Coor(jd,GUST86_OBERON,xyz);
}

