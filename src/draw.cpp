/*
 * Stellarium
 * Copyright (C) 2002 Fabien Chereau
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "draw.h"
#include "s_texture.h"
#include "stel_utility.h"

// rms added color as parameter
SkyGrid::SkyGrid(SKY_GRID_TYPE grid_type, unsigned int _nb_meridian, unsigned int _nb_parallel, double _radius,
	unsigned int _nb_alt_segment, unsigned int _nb_azi_segment) :
	nb_meridian(_nb_meridian), nb_parallel(_nb_parallel), 	radius(_radius),
	nb_alt_segment(_nb_alt_segment), nb_azi_segment(_nb_azi_segment), color(0.2,0.2,0.2)
{
	transparent_top = true;
	gtype = grid_type;
	switch (grid_type)
	{
		case ALTAZIMUTAL : proj_func = &Projector::project_local; break;
		case EQUATORIAL : proj_func = &Projector::project_earth_equ; break;
		default : proj_func = &Projector::project_earth_equ;
	}

	// Alt points are the points to draw along the meridian
	alt_points = new Vec3f*[nb_meridian];
	for (unsigned int nm=0;nm<nb_meridian;++nm)
	{
		alt_points[nm] = new Vec3f[nb_alt_segment+1];
		for (unsigned int i=0;i<nb_alt_segment+1;++i)
		{
			sphe_to_rect((float)nm/(nb_meridian)*2.f*M_PI,
				(float)i/nb_alt_segment*M_PI-M_PI_2, alt_points[nm][i]);
			alt_points[nm][i] *= radius;
		}
	}

	// Alt points are the points to draw along the meridian
	azi_points = new Vec3f*[nb_parallel];
	for (unsigned int np=0;np<nb_parallel;++np)
	{
		azi_points[np] = new Vec3f[nb_azi_segment+1];
		for (unsigned int i=0;i<nb_azi_segment+1;++i)
		{
			sphe_to_rect((float)i/(nb_azi_segment)*2.f*M_PI,
				(float)(np+1)/(nb_parallel+1)*M_PI-M_PI_2, azi_points[np][i]);
			azi_points[np][i] *= radius;
		}
	}
}

SkyGrid::~SkyGrid()
{
	for (unsigned int nm=0;nm<nb_meridian;++nm)
	{
		delete [] alt_points[nm];
	}
	delete [] alt_points;
	
	for (unsigned int np=0;np<nb_parallel;++np)
	{
		delete [] azi_points[np];
	}
	delete [] azi_points;
	
	if (font) delete font;
	font = NULL;

}

void SkyGrid::set_font(float font_size, const string& font_name)
{
	font = new s_font(font_size, font_name);
	assert(font);
}

void SkyGrid::draw(const Projector* prj) const
{
	if (!fader.get_interstate()) return;

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Normal transparency mode
	
	Vec3d pt1;
	Vec3d pt2;

	prj->set_orthographic_projection();	// set 2D coordinate

	// Draw meridians
	for (unsigned int nm=0;nm<nb_meridian;++nm)
	{
		if (transparent_top)	// Transparency for the first and last points
		{
			if ((prj->*proj_func)(alt_points[nm][0], pt1) &&
				(prj->*proj_func)(alt_points[nm][1], pt2) )
			{
				glColor4f(color[0],color[1],color[2],0.f);

				glBegin (GL_LINES);
					glVertex2f(pt1[0],pt1[1]);
					glColor4f(color[0],color[1],color[2],fader.get_interstate());
					glVertex2f(pt2[0],pt2[1]);
        		glEnd();
			}

			glColor4f(color[0],color[1],color[2],fader.get_interstate());

			for (unsigned int i=1;i<nb_alt_segment-1;++i)
			{
				if ((prj->*proj_func)(alt_points[nm][i], pt1) &&
					(prj->*proj_func)(alt_points[nm][i+1], pt2) )
				{
				  glBegin(GL_LINES);
				  glVertex2f(pt1[0],pt1[1]);
				  glVertex2f(pt2[0],pt2[1]);
				  glEnd();
				  
				  static char str[255];	// TODO use c++ string 
				  
				  glEnable(GL_TEXTURE_2D);

				  double angle;
				  double d;

				  // TODO: allow for other numbers of meridians and parallels without
				  // screwing up labels?
				  if( gtype == EQUATORIAL && i == 8 ) {

				    // draw labels along equator for RA
				    d = sqrt( (pt1[0]-pt2[0])*(pt1[0]-pt2[0]) + (pt1[1]-pt2[1])*(pt1[1]-pt2[1]) );
				  
				    angle = acos((pt1[1]-pt2[1])/d);
				    if( pt1[0] < pt2[0] ) {
				      angle *= -1;
				    }

				    sprintf( str, "%dh", nm);

				    prj->set_orthographic_projection();
				  
				    glTranslatef(pt2[0],pt2[1],0);
				    glRotatef(90+angle*180./M_PI,0,0,-1);
				    font->print(2,-2,str);

				    prj->reset_perspective_projection();


				  } else if (nm % 8 == 0 && i != 16) {
					  
				    d = sqrt( (pt1[0]-pt2[0])*(pt1[0]-pt2[0]) + (pt1[1]-pt2[1])*(pt1[1]-pt2[1]) );
				  
				    angle = acos((pt1[1]-pt2[1])/d);
				    if( pt1[0] < pt2[0] ) {
				      angle *= -1;
				    }

				    sprintf( str, "%d", (i-8)*10);
					    
				    if( gtype == ALTAZIMUTAL || 
					(gtype == EQUATORIAL && i > 8)) {
				      angle += M_PI;
				    }

				    prj->set_orthographic_projection();

				    glTranslatef(pt2[0],pt2[1],0);
				    glRotatef(angle*180./M_PI,0,0,-1);
				    font->print(2,-2,str);
				    prj->reset_perspective_projection();

				  }
				  glDisable(GL_TEXTURE_2D);
				  
				}


			}

			if ((prj->*proj_func)(alt_points[nm][nb_alt_segment-1], pt1) &&
				(prj->*proj_func)(alt_points[nm][nb_alt_segment], pt2) )
			{
				glColor4f(color[0],color[1],color[2],fader.get_interstate());
				glBegin (GL_LINES);
					glVertex2f(pt1[0],pt1[1]);
					glColor4f(color[0],color[1],color[2],0.f);
					glVertex2f(pt2[0],pt2[1]);
        		glEnd();
			}

		}
		else
		{
			glColor4f(color[0],color[1],color[2],fader.get_interstate());
			for (unsigned int i=0;i<nb_alt_segment;++i)
			{
				if ((prj->*proj_func)(alt_points[nm][i], pt1) &&
					(prj->*proj_func)(alt_points[nm][i+1], pt2) )
				{
					glBegin (GL_LINES);
						glVertex2f(pt1[0],pt1[1]);
						glVertex2f(pt2[0],pt2[1]);
        			glEnd();
				}
			}
		}
	}

	// Draw parallels
	glColor4f(color[0],color[1],color[2],fader.get_interstate());
	for (unsigned int np=0;np<nb_parallel;++np)
	{
		for (unsigned int i=0;i<nb_azi_segment;++i)
		{
			if ((prj->*proj_func)(azi_points[np][i], pt1) &&
				(prj->*proj_func)(azi_points[np][i+1], pt2) )
			{
				glBegin (GL_LINES);
					glVertex2f(pt1[0],pt1[1]);
					glVertex2f(pt2[0],pt2[1]);
        		glEnd();
			}
		}
	}

	prj->reset_perspective_projection();
}


SkyLine::SkyLine(SKY_LINE_TYPE _line_type, double _radius, unsigned int _nb_segment) :
	radius(_radius), nb_segment(_nb_segment), color(0.f, 0.f, 1.f)
{
	float inclinaison = 0.f;
	line_type = _line_type;

	switch (line_type)
		{
		case LOCAL : proj_func = &Projector::project_local; break;
		case MERIDIAN : proj_func = &Projector::project_local; 
			inclinaison = 90; break;
		case ECLIPTIC : proj_func = &Projector::project_earth_equ;
			inclinaison = 23.45f; break;
		case EQUATOR : proj_func = &Projector::project_earth_equ; break;
		default : proj_func = &Projector::project_earth_equ;
	}

	Mat4f r = Mat4f::xrotation(inclinaison*M_PI/180.f);

	// Points to draw along the circle
	points = new Vec3f[nb_segment+1];
	for (unsigned int i=0;i<nb_segment+1;++i)
	{
		sphe_to_rect((float)i/(nb_segment)*2.f*M_PI, 0.f, points[i]);
		points[i] *= radius;
		points[i].transfo4d(r);
	}
}

SkyLine::~SkyLine()
{
	delete [] points;
	points = NULL;
}

void SkyLine::set_font(float font_size, const string& font_name)
{
	font = new s_font(font_size, font_name);
	assert(font);
}

void SkyLine::draw(const Projector* prj) const
{
	if (!fader.get_interstate()) return;

	Vec3d pt1;
	Vec3d pt2;

	glColor4f(color[0], color[1], color[2], fader.get_interstate());
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Normal transparency mode

	prj->set_orthographic_projection();	// set 2D coordinate

	for (unsigned int i=0;i<nb_segment;++i)
	{
		if ((prj->*proj_func)(points[i], pt1) &&
			(prj->*proj_func)(points[i+1], pt2) )
		{

			double angle;
			double d;

			// TODO: allow for other numbers of meridians and parallels without
			// screwing up labels?

			glBegin (GL_LINES);
				glVertex2f(pt1[0],pt1[1]);
				glVertex2f(pt2[0],pt2[1]);
       		glEnd();


			if(line_type == MERIDIAN) {
				d = sqrt( (pt1[0]-pt2[0])*(pt1[0]-pt2[0]) + (pt1[1]-pt2[1])*(pt1[1]-pt2[1]) );
				  
				angle = acos((pt1[1]-pt2[1])/d);
				if( pt1[0] < pt2[0] ) {
					angle *= -1;
				}

				// draw text label
				std::ostringstream oss;	
				
				if(i<=8) oss << (i+1)*10;
				else if(i<=16) {
					oss << (17-i)*10;
					angle += M_PI;
				}
				else oss << "";
				
				glPushMatrix();
				glTranslatef(pt2[0],pt2[1],0);
				glRotatef(180+angle*180./M_PI,0,0,-1);
				
				glBegin (GL_LINES);
				glVertex2f(-3,0);
				glVertex2f(3,0);
				glEnd();
				glEnable(GL_TEXTURE_2D);

				if(font) font->print(2,-2,oss.str());
				glPopMatrix();
				glDisable(GL_TEXTURE_2D);

			}
		}
	}

	prj->reset_perspective_projection();
}


Cardinals::Cardinals(float _radius) : radius(_radius), font(NULL), color(0.6,0.2,0.2)
{
	// Default labels - if sky locale specified, loaded later
	// Improvement for gettext translation
	sNorth = _("N");
	sSouth = _("S");
	sEast = _("E");
	sWest = _("W");
}

Cardinals::~Cardinals()
{
    if (font) delete font;
	font = NULL;
}

void Cardinals::set_font(float font_size, const string& font_name)
{
	font = new s_font(font_size, font_name);
	assert(font);
}

// Draw the cardinals points : N S E W
// handles special cases at poles
void Cardinals::draw(const Projector* prj, double latitude, bool gravityON) const
{

	if (!fader.get_interstate()) return;

	// direction text
	string d[4];
	
	d[0] = sNorth;
	d[1] = sSouth;
	d[2] = sEast;
	d[3] = sWest;
	
	// fun polar special cases
	if(latitude ==  90.0 ) d[0] = d[1] = d[2] = d[3] = sSouth;
	if(latitude == -90.0 ) d[0] = d[1] = d[2] = d[3] = sNorth;

	glColor4f(color[0],color[1],color[2],fader.get_interstate());
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	// Normal transparency mode
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Vec3f pos;
	Vec3d xy;

	prj->set_orthographic_projection();

	float shift = font->getStrLen(sNorth)/2;

	if (gravityON)
	{
		// N for North
		pos.set(-1.f, 0.f, 0.22f);
		if (prj->project_local(pos,xy)) prj->print_gravity180(font, xy[0], xy[1], d[0], -shift, -shift);

		// S for South
		pos.set(1.f, 0.f, 0.22f);
		if (prj->project_local(pos,xy)) prj->print_gravity180(font, xy[0], xy[1], d[1], -shift, -shift);

		// E for East
		pos.set(0.f, 1.f, 0.22f);
		if (prj->project_local(pos,xy)) prj->print_gravity180(font, xy[0], xy[1], d[2], -shift, -shift);

		// W for West
		pos.set(0.f, -1.f, 0.22f);
		if (prj->project_local(pos,xy)) prj->print_gravity180(font, xy[0], xy[1], d[3], -shift, -shift);
	}
	else
	{
		// N for North
		pos.set(-1.f, 0.f, 0.f);
		if (prj->project_local(pos,xy)) font->print(xy[0]-shift, xy[1]-shift, d[0]);

		// S for South
		pos.set(1.f, 0.f, 0.f);
		if (prj->project_local(pos,xy)) font->print(xy[0]-shift, xy[1]-shift, d[1]);

		// E for East
		pos.set(0.f, 1.f, 0.f);
		if (prj->project_local(pos,xy)) font->print(xy[0]-shift, xy[1]-shift, d[2]);

		// W for West
		pos.set(0.f, -1.f, 0.f);
		if (prj->project_local(pos,xy)) font->print(xy[0]-shift, xy[1]-shift, d[3]);
	}

	prj->reset_perspective_projection();
}

// load cardinal labels from a file for i18n to sky language
// (not using gettext because sky language not the same as ui language)

int Cardinals::load_labels(string filename)
{
    ifstream input_file;

	input_file.open(filename.c_str());

	if (! input_file.is_open())
	{
		printf("Error opening %s\n", filename.c_str());

		// Default labels - if sky locale specified, loaded later
		// Improvement for gettext translation
		sNorth = _("N");
		sSouth = _("S");
		sEast = _("E");
		sWest = _("W");

		return 0;
	}

	input_file >> sNorth >> sEast >> sSouth >> sWest;
	return 1;
}

// Class which manages the displaying of the Milky Way
MilkyWay::MilkyWay(float _radius) : radius(_radius), color(1.f, 1.f, 1.f)
{
	tex = NULL;
}

MilkyWay::~MilkyWay()
{
	if (tex) delete tex;
	tex = NULL;
}

void MilkyWay::set_texture(const string& tex_file)
{
	if (tex) delete tex;
	tex = new s_texture(tex_file,TEX_LOAD_TYPE_PNG_SOLID_REPEAT);

	// big performance improvement to cache this
	tex_avg_luminance = tex->get_average_luminance();
}


void MilkyWay::set_intensity(float _intensity)
{
	intensity = _intensity;
}

void MilkyWay::draw(tone_reproductor * eye, const Projector* prj, const navigator* nav) const
{
	assert(tex);	// A texture must be loaded before calling this
	// Scotopic color = 0.25, 0.25 in xyY mode. Global stars luminance ~= 0.001 cd/m^2
	Vec3f c = Vec3f(0.25f, 0.25f, intensity*0.002f);
	eye->xyY_to_RGB(c);
	glColor3fv(c);
	c[2]/=tex_avg_luminance;
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, tex->getID());

	prj->sSphere(radius,20,20,
		     nav->get_prec_earth_equ_to_eye_mat()*
		     Mat4d::xrotation(M_PI/180*23)*
		     Mat4d::yrotation(M_PI/180*120)*
		     Mat4d::zrotation(M_PI/180*7), 1);

	glDisable(GL_CULL_FACE);
}


// Draw a point... (used for tests)
void DrawPoint(float X,float Y,float Z)
{       
	glColor3f(0.8, 1.0, 0.8);
	glDisable(GL_TEXTURE_2D);
	//glEnable(GL_BLEND);
	glPointSize(20.);
	glBegin(GL_POINTS);
		glVertex3f(X,Y,Z);
	glEnd();
}
