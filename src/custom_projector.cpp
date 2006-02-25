/*
 * Stellarium
 * Copyright (C) 2003 Fabien Chereau
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

#include <cstdio>
#include <iostream>
#include "custom_projector.h"


CustomProjector::CustomProjector(int _screenW, int _screenH, double _fov)
                :Projector(_screenW, _screenH, _fov)
{
	set_screen_size(_screenW,_screenH);
	mat_projection.set(1., 0., 0., 0.,
							0., 1., 0., 0.,
							0., 0., -1, 0.,
							0., 0., 0., 1.);				
}

// For a fisheye, ratio is alway = 1
void CustomProjector::setViewport(int x, int y, int w, int h)
{
	Projector::setViewport(x, y, w, h);
	center.set(vec_viewport[0]+vec_viewport[2]/2,vec_viewport[1]+vec_viewport[3]/2,0);
}

// Init the viewing matrix, setting the field of view, the clipping planes, and screen ratio
// The function is a reimplementation of glOrtho
void CustomProjector::init_project_matrix(void)
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(mat_projection);
    glMatrixMode(GL_MODELVIEW);
}

void CustomProjector::update_openGL(void) const
{
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(mat_projection);
    glMatrixMode(GL_MODELVIEW);
	glViewport(vec_viewport[0], vec_viewport[1], vec_viewport[2], vec_viewport[3]);
}

void CustomProjector::unproject_custom(double x ,double y, Vec3d& v, const Mat4d& mat) const
{
	unproject(x, y, (mat_projection*mat).inverse(), v);
}

// Override glVertex3f
// Here is the main trick for texturing in fisheye mode : The trick is to compute the
// new coordinate in orthographic projection which will simulate the fisheye projection.
void CustomProjector::sVertex3(double x, double y, double z, const Mat4d& mat) const
{
	Vec3d win;
	Vec3d v(x,y,z);
	project_custom(v, win, mat);

	// Can be optimized by avoiding matrix inversion if it's always the same
	gluUnProject(win[0],win[1],win[2],mat,mat_projection,vec_viewport,&v[0],&v[1],&v[2]);
	glVertex3dv(v);
}

void CustomProjector::sSphere(GLdouble radius, GLint slices, GLint stacks, const Mat4d& mat, int orient_inside) const
{
	glPushMatrix();
	glLoadMatrixd(mat);

	static Vec4f lightPos4;
	static Vec3f lightPos3;
	static GLboolean isLightOn;
	static Vec3f transNorm;
	static float c;

	static Vec4f ambientLight;
	static Vec4f diffuseLight;

	static Vec3d posCenterEye;
	posCenterEye = mat * Vec3d(0.,0.,0.);

	glGetBooleanv(GL_LIGHTING, &isLightOn);

	if (isLightOn)
	{
		glGetLightfv(GL_LIGHT0, GL_POSITION, lightPos4);
		lightPos3 = lightPos4;
		lightPos3-=posCenterEye;
		lightPos3.normalize();
		glGetLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
		glGetLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
		glDisable(GL_LIGHTING);
	}

	static GLfloat rho, drho, theta, dtheta;
	static GLfloat x, y, z;
	static GLfloat s, t, ds, dt;
	static GLint i, j, imin, imax;
	static GLfloat nsign;

	if (orient_inside) {
	  nsign = -1.0;
	  t=0.0; // from inside texture is reversed
	} else {
	  nsign = 1.0;
	  t=1.0;
	}

	drho = M_PI / (GLfloat) stacks;
	dtheta = 2.0 * M_PI / (GLfloat) slices;

	// texturing: s goes from 0.0/0.25/0.5/0.75/1.0 at +y/+x/-y/-x/+y axis
	// t goes from -1.0/+1.0 at z = -radius/+radius (linear along longitudes)
	// cannot use triangle fan on texturing (s coord. at top/bottom tip varies)
	ds = 1.0 / slices;
	dt = nsign / stacks; // from inside texture is reversed

	imin = 0;
	imax = stacks;

	// draw intermediate stacks as quad strips
	for (i = imin; i < imax; i++)
	{
		rho = i * drho;
		glBegin(GL_QUAD_STRIP);
		s = 0.0;
		for (j = 0; j <= slices; j++)
		{
			theta = (j == slices) ? 0.0 : j * dtheta;
			x = -sin(theta) * sin(rho);
			y = cos(theta) * sin(rho);
			z = nsign * cos(rho);
			//glNormal3f(x * nsign, y * nsign, z * nsign);
			glTexCoord2f(s, t);
			if (isLightOn)
			{
				transNorm = mat*Vec3d(x * nsign, y * nsign, z * nsign) - posCenterEye;
				transNorm.normalize();
				c = lightPos3.dot(transNorm);
				if (c<0) c=0;
				glColor3f(c*diffuseLight[0] + ambientLight[0],
					c*diffuseLight[1] + ambientLight[1],
					c*diffuseLight[2] + ambientLight[2]);
			}
			sVertex3(x * radius, y * radius, z * radius, mat);
			x = -sin(theta) * sin(rho + drho);
			y = cos(theta) * sin(rho + drho);
			z = nsign * cos(rho + drho);
			//glNormal3f(x * nsign, y * nsign, z * nsign);
			glTexCoord2f(s, t - dt);
			if (isLightOn)
			{
				transNorm = mat*Vec3d(x * nsign, y * nsign, z * nsign) - posCenterEye;
				transNorm.normalize();
				c = lightPos3.dot(transNorm);
				if (c<0) c=0;
				glColor3f(c*diffuseLight[0] + ambientLight[0],
					c*diffuseLight[1] + ambientLight[1],
					c*diffuseLight[2] + ambientLight[2]);
			}
			sVertex3(x * radius, y * radius, z * radius, mat);
			s += ds;
		}
		glEnd();
		t -= dt;
	}
	glPopMatrix();
	if (isLightOn) glEnable(GL_LIGHTING);
}

// Reimplementation of gluCylinder : glu is overrided for non standard projection
void CustomProjector::sCylinder(GLdouble radius, GLdouble height, GLint slices, GLint stacks,
const Mat4d& mat, int orient_inside) const
{
	glPushMatrix();
	glLoadMatrixd(mat);

	static GLdouble da, r, dz;
	static GLfloat z, nsign;
	static GLint i, j;

	nsign = 1.0;
	if (orient_inside) glCullFace(GL_FRONT);
	//nsign = -1.0;
	//else nsign = 1.0;

	da = 2.0 * M_PI / slices;
	dz = height / stacks;

	GLfloat ds = 1.0 / slices;
	GLfloat dt = 1.0 / stacks;
	GLfloat t = 0.0;
	z = 0.0;
	r = radius;
	for (j = 0; j < stacks; j++)
	{
	GLfloat s = 0.0;
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= slices; i++)
	{
		GLfloat x, y;
		if (i == slices)
		{
			x = sinf(0.0);
			y = cosf(0.0);
		}
		else
		{
			x = sinf(i * da);
			y = cosf(i * da);
		}
		glNormal3f(x * nsign, y * nsign, 0);
		glTexCoord2f(s, t);
		sVertex3(x * r, y * r, z, mat);
		glNormal3f(x * nsign, y * nsign, 0);
		glTexCoord2f(s, t + dt);
		sVertex3(x * r, y * r, z + dz, mat);
		s += ds;
	}			/* for slices */
	glEnd();
	t += dt;
	z += dz;
	}				/* for stacks */

	glPopMatrix();
	if (orient_inside) glCullFace(GL_BACK);
}
