/*
 * Stellarium
 * Copyright (C) 2008 Fabien Chereau
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include "StelPainter.hpp"
#include <QtOpenGL>

#include "StelApp.hpp"
#include "StelLocaleMgr.hpp"
#include "StelProjector.hpp"
#include "StelProjectorClasses.hpp"
#include "StelUtils.hpp"

#include <QDebug>
#include <QString>
#include <QSettings>
#include <QLinkedList>
#include <QPainter>
#include <QMutex>
#include <QVarLengthArray>
#include <QPaintEngine>

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE  0x809D
#endif

#ifndef NDEBUG
QMutex* StelPainter::globalMutex = new QMutex();
#endif

QPainter* StelPainter::qPainter = NULL;
QGLContext* StelPainter::glContext = NULL;

bool StelPainter::isNoPowerOfTwoAllowed;

#ifdef STELPAINTER_GL2
 QGLShaderProgram* StelPainter::colorShaderProgram=NULL;
 QGLShaderProgram* StelPainter::texturesShaderProgram=NULL;
 QGLShaderProgram* StelPainter::basicShaderProgram=NULL;
 QGLShaderProgram* StelPainter::texturesColorShaderProgram=NULL;
 StelPainter::BasicShaderVars StelPainter::basicShaderVars;
 StelPainter::TexturesShaderVars StelPainter::texturesShaderVars;
 StelPainter::TexturesColorShaderVars StelPainter::texturesColorShaderVars;
#endif

		//QPainter qPainter(glWidget);
void StelPainter::setQPainter(QPainter* p)
{
	qPainter=p;
	if (p==NULL)
		return;

	if (p->paintEngine()->type() != QPaintEngine::OpenGL && p->paintEngine()->type() != QPaintEngine::OpenGL2)
	{
		qCritical("StelPainter::setQPainter(): StelPainter needs a QGLWidget to be set as viewport on the graphics view");
		return;
	}
	QGLWidget* glwidget = dynamic_cast<QGLWidget*>(p->device());
	if (glwidget && glwidget->context()!=glContext)
	{
		qCritical("StelPainter::setQPainter(): StelPainter needs to paint on a GLWidget with the same GL context as the one used for initialization.");
		return;
	}
}

void StelPainter::makeMainGLContextCurrent()
{
	Q_ASSERT(glContext!=NULL);
	Q_ASSERT(glContext->isValid());
	glContext->makeCurrent();
}

void StelPainter::swapBuffer()
{
	Q_ASSERT(glContext!=NULL);
	Q_ASSERT(glContext->isValid());
	glContext->swapBuffers();
}

StelPainter::StelPainter(const StelProjectorP& proj) : prj(proj)
{
	Q_ASSERT(proj);

#ifndef NDEBUG
	Q_ASSERT(globalMutex);
	GLenum er = glGetError();
	if (er!=GL_NO_ERROR)
	{
		if (er==GL_INVALID_OPERATION)
			qFatal("Invalid openGL operation. It is likely that you used openGL calls without having a valid instance of StelPainter");
	}

	// Lock the global mutex ensuring that no other instances of StelPainter are currently being used
	if (globalMutex->tryLock()==false)
	{
		qFatal("There can be only 1 instance of StelPainter at a given time");
	}
#endif

	Q_ASSERT(qPainter);
	qPainter->beginNativePainting();

#ifndef STELPAINTER_GL2
	// Save openGL projection state
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	

	glDisable(GL_LIGHTING);
	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DITHER);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_LINE_SMOOTH);
#endif
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	// Fix some problem when using Qt OpenGL2 engine
	glStencilMask(0x11111111);
	// Deactivate drawing in depth buffer by default
	glDepthMask(GL_FALSE);
	enableTexture2d(false);
	setShadeModel(StelPainter::ShadeModelFlat);
	setProjector(proj);
}

void StelPainter::setProjector(const StelProjectorP& p)
{
	prj=p;
	// Init GL viewport to current projector values
	glViewport(prj->viewportXywh[0], prj->viewportXywh[1], prj->viewportXywh[2], prj->viewportXywh[3]);
	glFrontFace(prj->flipFrontBackFace()?GL_CW:GL_CCW);
#ifndef STELPAINTER_GL2
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	// Set the real openGL projection and modelview matrix to 2d orthographic projection
	// thus we never need to change to 2dMode from now on before drawing
	glMultMatrixf(prj->getProjectionMatrix());
	glMatrixMode(GL_MODELVIEW);
#endif
}

StelPainter::~StelPainter()
{
	Q_ASSERT(qPainter);

#ifndef STELPAINTER_GL2
	// Restore openGL projection state for Qt drawings
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
#endif
#ifndef NDEBUG
	GLenum er = glGetError();
	if (er!=GL_NO_ERROR)
	{
		if (er==GL_INVALID_OPERATION)
			qFatal("Invalid openGL operation detected in ~StelPainter()");
	}
#endif
	qPainter->endNativePainting();

#ifndef NDEBUG
	// We are done with this StelPainter
	globalMutex->unlock();
#endif
}


void StelPainter::setFont(const QFont& font)
{
	Q_ASSERT(qPainter);
	qPainter->setFont(font);
}

void StelPainter::setColor(float r, float g, float b, float a)
{
#ifndef STELPAINTER_GL2
	glColor4f(r,g,b,a);
#else
	currentColor.set(r,g,b,a);
#endif
}

Vec4f StelPainter::getColor() const
{
#ifndef STELPAINTER_GL2
	GLfloat tmpColor[4];
	glGetFloatv(GL_CURRENT_COLOR, tmpColor);
	return Vec4f(tmpColor[0], tmpColor[1], tmpColor[2], tmpColor[3]);
#else
	return currentColor;
#endif
}

QFontMetrics StelPainter::getFontMetrics() const
{
	Q_ASSERT(qPainter);
	return qPainter->fontMetrics();
}


///////////////////////////////////////////////////////////////////////////
// Standard methods for drawing primitives

void StelPainter::drawTextGravity180(float x, float y, const QString& ws, float xshift, float yshift)
{
	float dx, dy, d, theta, psi;
	dx = x - prj->viewportCenter[0];
	dy = y - prj->viewportCenter[1];
	d = std::sqrt(dx*dx + dy*dy);

	// If the text is too far away to be visible in the screen return
	if (d>qMax(prj->viewportXywh[3], prj->viewportXywh[2])*2)
		return;
	theta = std::atan2(dy - 1, dx);
	psi = std::atan2((float)qPainter->fontMetrics().width(ws)/ws.length(),d + 1) * 180./M_PI;
	if (psi>5) {
		psi = 5;
	}

	float cWidth = (float)qPainter->fontMetrics().width(ws)/ws.length();
	float xVc = prj->viewportCenter[0] + xshift;
	float yVc = prj->viewportCenter[1] + yshift;

	QString lang = StelApp::getInstance().getLocaleMgr().getAppLanguage();
	if (!QString("ar fa ur he yi").contains(lang)) {
        	for (int i=0; i<ws.length(); ++i)
	        {
			x = d * std::cos (theta) + xVc ;
			y = d * std::sin (theta) + yVc ; 
			drawText(x, y, ws[i], 90. + theta*180./M_PI, 0., 0.);
			// Compute how much the character contributes to the angle
			theta += psi * M_PI/180. * (1 + ((float)qPainter->fontMetrics().width(ws[i]) - cWidth)/ cWidth);
		}
	}
	else
	{
		int slen = ws.length();
		for (int i=0;i<slen;i++)
		{
			x = d * std::cos (theta) + xVc;
			y = d * std::sin (theta) + yVc; 
			drawText(x, y, ws[slen-1-i], 90. + theta*180./M_PI, 0., 0.);
			theta += psi * M_PI/180. * (1 + ((float)qPainter->fontMetrics().width(ws[slen-1-i]) - cWidth)/ cWidth);
		}
	}
}

void StelPainter::drawText(const Vec3d& v, const QString& str, float angleDeg, float xshift, float yshift, bool noGravity)
{
	Vec3d win;
	prj->project(v, win);
	drawText(win[0], win[1], str, angleDeg, xshift, yshift, noGravity);
}

/*************************************************************************
 Draw the string at the given position and angle with the given font
*************************************************************************/

// Container for one cached string texture
struct StringTexture
{
	GLuint texture;
	int width;
	int height;
	int subTexWidth;
	int subTexHeight;

	StringTexture() : texture(0) {;}
	~StringTexture()
	{
		if (texture != 0)
			glDeleteTextures(1, &texture);
	}
};


void StelPainter::drawText(float x, float y, const QString& str, float angleDeg, float xshift, float yshift, bool noGravity)
{
	Q_ASSERT(qPainter);
	if (prj->gravityLabels && !noGravity)
	{
		drawTextGravity180(x, y, str, xshift, yshift);
	}
	else
	{
		static const int cacheLimitByte = 10000000;
		static QCache<QByteArray,StringTexture> texCache(cacheLimitByte);
		int pixelSize = qPainter->font().pixelSize();
		QByteArray hash = str.toUtf8() + QByteArray::number(pixelSize);

		const StringTexture* cachedTex = texCache.object(hash);
		if (cachedTex == NULL)	// need to create texture
		{
			StringTexture* newTex = new StringTexture();

			// Create temp image and render text into it
			QRect strRect = getFontMetrics().boundingRect(str);
			newTex->subTexWidth = strRect.width()+1+(int)(0.02f*strRect.width());
			newTex->subTexHeight = strRect.height();
			QPixmap strImage;
			if (isNoPowerOfTwoAllowed)
				strImage = QPixmap(newTex->subTexWidth, newTex->subTexHeight);
			else
				strImage = QPixmap(StelUtils::smallestPowerOfTwoGreaterOrEqualTo(newTex->subTexWidth), 
				                   StelUtils::smallestPowerOfTwoGreaterOrEqualTo(newTex->subTexHeight));
			newTex->width = strImage.width();
			newTex->height = strImage.height();

			strImage.fill(Qt::transparent);

			QPainter painter(&strImage);
			painter.setFont(qPainter->font());
			painter.setRenderHints(QPainter::TextAntialiasing, true);
			painter.setPen(Qt::white);
			painter.drawText(-strRect.x(), -strRect.y(), str);

			// Create and bind texture, and add it to the list of cached textures
			newTex->texture = StelPainter::glContext->bindTexture(strImage, GL_TEXTURE_2D, GL_RGBA, QGLContext::NoBindOption);
			texCache.insert(hash, newTex, 3*newTex->width*newTex->height);
			cachedTex=newTex;
		}
		else
		{
			// The texture was found in the cache
			glBindTexture(GL_TEXTURE_2D, cachedTex->texture);
		}

		// Translate/rotate
		if (!noGravity)
			angleDeg += prj->defautAngleForGravityText;

		enableTexture2d(true);
		static float vertexData[8];
		static const float texCoordData[] = {0.,1., 1.,1., 0.,0., 1.,0.};
		// compute the vertex coordinates applying the translation and the rotation
		static const float vertexBase[] = {0., 0., 1., 0., 0., 1., 1., 1.};
		if (std::fabs(angleDeg)>1.f*M_PI/180.f)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			const float cosr = std::cos(angleDeg * M_PI/180.);
			const float sinr = std::sin(angleDeg * M_PI/180.);
			for (int i = 0; i < 8; i+=2)
			{
				vertexData[i] = int(x + (cachedTex->subTexWidth*vertexBase[i]+xshift) * cosr - (cachedTex->subTexHeight*vertexBase[i+1]+yshift) * sinr);
				vertexData[i+1] = int(y  + (cachedTex->subTexWidth*vertexBase[i]+xshift) * sinr + (cachedTex->subTexHeight*vertexBase[i+1]+yshift) * cosr);
			}
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			for (int i = 0; i < 8; i+=2)
			{
				vertexData[i] = int(x + cachedTex->subTexWidth*vertexBase[i]+xshift);
				vertexData[i+1] = int(y  + cachedTex->subTexHeight*vertexBase[i+1]+yshift);
			}
		}


		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		enableClientStates(true, true);
		setVertexPointer(2, GL_FLOAT, vertexData);

		float* texCoords = NULL;
		if (isNoPowerOfTwoAllowed)
			setTexCoordPointer(2, GL_FLOAT, texCoordData);
		else
		{
			texCoords = new float[8];
			for (int i=0;i<8;i+=2)
			{
				texCoords[i] = texCoordData[i]*(float)cachedTex->subTexWidth/cachedTex->width;
				texCoords[i+1] = texCoordData[i+1]*(float)cachedTex->subTexHeight/cachedTex->height;
			}
			setTexCoordPointer(2, GL_FLOAT, texCoords);
		}

		drawFromArray(TriangleStrip, 4, 0, false);
		enableClientStates(false);

		if (!isNoPowerOfTwoAllowed)
			delete[] texCoords;
	}
}

// Recursive method cutting a small circle in small segments
inline void fIter(const StelProjectorP& prj, const Vec3d& p1, const Vec3d& p2, Vec3d& win1, Vec3d& win2, QLinkedList<Vec3d>& vertexList, const QLinkedList<Vec3d>::iterator& iter, double radius, const Vec3d& center, int nbI=0, bool checkCrossDiscontinuity=true)
{
	const bool crossDiscontinuity = checkCrossDiscontinuity && prj->intersectViewportDiscontinuity(p1+center, p2+center);
	if (crossDiscontinuity && nbI>=10)
	{
		win1[2]=-2.;
		win2[2]=-2.;
		vertexList.insert(iter, win1);
		vertexList.insert(iter, win2);
		return;
	}

	Vec3d newVertex(p1); newVertex+=p2;
	newVertex.normalize();
	newVertex*=radius;
	Vec3d win3(newVertex[0]+center[0], newVertex[1]+center[1], newVertex[2]+center[2]);
	const bool isValidVertex = prj->projectInPlace(win3);

	const float v10=win1[0]-win3[0];
	const float v11=win1[1]-win3[1];
	const float v20=win2[0]-win3[0];
	const float v21=win2[1]-win3[1];

	const float dist = std::sqrt((v10*v10+v11*v11)*(v20*v20+v21*v21));
	const float cosAngle = (v10*v20+v11*v21)/dist;
	if ((cosAngle>-0.999f || dist>50*50 || crossDiscontinuity) && nbI<10)
	{
		// Use the 3rd component of the vector to store whether the vertex is valid
		win3[2]= isValidVertex ? 1.0 : -1.;
		fIter(prj, p1, newVertex, win1, win3, vertexList, vertexList.insert(iter, win3), radius, center, nbI+1, crossDiscontinuity || dist>50*50);
		fIter(prj, newVertex, p2, win3, win2, vertexList, iter, radius, center, nbI+1, crossDiscontinuity || dist>50*50 );
	}
}

// Project the passed triangle on the screen ensuring that it will look smooth, even for non linear distortion
// by splitting it into subtriangles.
void StelPainter::projectSphericalTriangle(const SphericalCap* clippingCap, const Vec3d* vertices, QVarLengthArray<Vec3f, 4096>* outVertices,
		const Vec2f* texturePos, QVarLengthArray<Vec2f, 4096>* outTexturePos,
		double maxSqDistortion, int nbI, bool checkDisc1, bool checkDisc2, bool checkDisc3) const
{
	Q_ASSERT(fabs(vertices[0].length()-1.)<0.00001);
	Q_ASSERT(fabs(vertices[1].length()-1.)<0.00001);
	Q_ASSERT(fabs(vertices[2].length()-1.)<0.00001);
	if (clippingCap && clippingCap->containsTriangle(vertices))
		clippingCap = NULL;
	if (clippingCap && !clippingCap->intersectsTriangle(vertices))
		return;
	bool cDiscontinuity1 = checkDisc1 && prj->intersectViewportDiscontinuity(vertices[0], vertices[1]);
	bool cDiscontinuity2 = checkDisc2 && prj->intersectViewportDiscontinuity(vertices[1], vertices[2]);
	bool cDiscontinuity3 = checkDisc3 && prj->intersectViewportDiscontinuity(vertices[0], vertices[2]);
	const bool cd1=cDiscontinuity1;
	const bool cd2=cDiscontinuity2;
	const bool cd3=cDiscontinuity3;

	Vec3d e0=vertices[0];
	Vec3d e1=vertices[1];
	Vec3d e2=vertices[2];
	bool valid = prj->projectInPlace(e0);
	valid = prj->projectInPlace(e1) || valid;
	valid = prj->projectInPlace(e2) || valid;
	// Clip polygons behind the viewer
	if (!valid)
		return;

	if (checkDisc1 && cDiscontinuity1==false)
	{
		// If the distortion at segment e0,e1 is too big, flags it for subdivision
		Vec3d win3 = vertices[0]; win3+=vertices[1];
		prj->projectInPlace(win3);
		win3[0]-=(e0[0]+e1[0])*0.5; win3[1]-=(e0[1]+e1[1])*0.5;
		cDiscontinuity1 = (win3[0]*win3[0]+win3[1]*win3[1])>maxSqDistortion;
	}
	if (checkDisc2 && cDiscontinuity2==false)
	{
		// If the distortion at segment e1,e2 is too big, flags it for subdivision
		Vec3d win3 = vertices[1]; win3+=vertices[2];
		prj->projectInPlace(win3);
		win3[0]-=(e2[0]+e1[0])*0.5; win3[1]-=(e2[1]+e1[1])*0.5;
		cDiscontinuity2 = (win3[0]*win3[0]+win3[1]*win3[1])>maxSqDistortion;
	}
	if (checkDisc3 && cDiscontinuity3==false)
	{
		// If the distortion at segment e2,e0 is too big, flags it for subdivision
		Vec3d win3 = vertices[2]; win3+=vertices[0];
		prj->projectInPlace(win3);
		win3[0] -= (e0[0]+e2[0])*0.5;
		win3[1] -= (e0[1]+e2[1])*0.5;
		cDiscontinuity3 = (win3[0]*win3[0]+win3[1]*win3[1])>maxSqDistortion;
	}

	if (!cDiscontinuity1 && !cDiscontinuity2 && !cDiscontinuity3)
	{
		// The triangle is clean, appends it
		outVertices->append(Vec3f(e0[0], e0[1], e0[2])); outVertices->append(Vec3f(e1[0], e1[1], e1[2])); outVertices->append(Vec3f(e2[0], e2[1], e2[2]));
		if (outTexturePos)
			outTexturePos->append(texturePos,3);
		return;
	}

	if (nbI > 4)
	{
		// If we reached the limit number of iterations and still have a discontinuity,
		// discards the triangle.
		if (cd1 || cd2 || cd3)
			return;

		// Else display it, it will be suboptimal though.
		outVertices->append(Vec3f(e0[0], e0[1], e0[2])); outVertices->append(Vec3f(e1[0], e1[1], e2[2])); outVertices->append(Vec3f(e2[0], e2[1], e2[2]));
		if (outTexturePos)
			outTexturePos->append(texturePos,3);
		return;
	}

	// Recursively splits the triangle into sub triangles.
	// Depending on which combination of sides of the triangle has to be split a different strategy is used.
	Vec3d va[3];
	Vec2f ta[3];
	// Only 1 side has to be split: split the triangle in 2
	if (cDiscontinuity1 && !cDiscontinuity2 && !cDiscontinuity3)
	{
		va[0]=vertices[0];
		va[1]=vertices[0];va[1]+=vertices[1];
		va[1].normalize();
		va[2]=vertices[2];
		if (outTexturePos)
		{
			ta[0]=texturePos[0];
			ta[1]=(texturePos[0]+texturePos[1])*0.5;
			ta[2]=texturePos[2];
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1, true, true, false);

		//va[0]=vertices[0]+vertices[1];
		//va[0].normalize();
		va[0]=va[1];
		va[1]=vertices[1];
		va[2]=vertices[2];
		if (outTexturePos)
		{
			ta[0]=(texturePos[0]+texturePos[1])*0.5;
			ta[1]=texturePos[1];
			ta[2]=texturePos[2];
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1, true, false, true);
		return;
	}

	if (!cDiscontinuity1 && cDiscontinuity2 && !cDiscontinuity3)
	{
		va[0]=vertices[0];
		va[1]=vertices[1];
		va[2]=vertices[1];va[2]+=vertices[2];
		va[2].normalize();
		if (outTexturePos)
		{
			ta[0]=texturePos[0];
			ta[1]=texturePos[1];
			ta[2]=(texturePos[1]+texturePos[2])*0.5;
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1, false, true, true);

		va[0]=vertices[0];
		//va[1]=vertices[1]+vertices[2];
		//va[1].normalize();
		va[1]=va[2];
		va[2]=vertices[2];
		if (outTexturePos)
		{
			ta[0]=texturePos[0];
			ta[1]=(texturePos[1]+texturePos[2])*0.5;
			ta[2]=texturePos[2];
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1, true, true, false);
		return;
	}

	if (!cDiscontinuity1 && !cDiscontinuity2 && cDiscontinuity3)
	{
		va[0]=vertices[0];
		va[1]=vertices[1];
		va[2]=vertices[0];va[2]+=vertices[2];
		va[2].normalize();
		if (outTexturePos)
		{
			ta[0]=texturePos[0];
			ta[1]=texturePos[1];
			ta[2]=(texturePos[0]+texturePos[2])*0.5;
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1, false, true, true);

		//va[0]=vertices[0]+vertices[2];
		//va[0].normalize();
		va[0]=va[2];
		va[1]=vertices[1];
		va[2]=vertices[2];
		if (outTexturePos)
		{
			ta[0]=(texturePos[0]+texturePos[2])*0.5;
			ta[1]=texturePos[1];
			ta[2]=texturePos[2];
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1, true, false, true);
		return;
	}

	// 2 sides have to be split: split the triangle in 3
	if (cDiscontinuity1 && cDiscontinuity2 && !cDiscontinuity3)
	{
		va[0]=vertices[0];
		va[1]=vertices[0];va[1]+=vertices[1];
		va[1].normalize();
		va[2]=vertices[1];va[2]+=vertices[2];
		va[2].normalize();
		if (outTexturePos)
		{
			ta[0]=texturePos[0];
			ta[1]=(texturePos[0]+texturePos[1])*0.5;
			ta[2]=(texturePos[1]+texturePos[2])*0.5;
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1);

		//va[0]=vertices[0]+vertices[1];
		//va[0].normalize();
		va[0]=va[1];
		va[1]=vertices[1];
		//va[2]=vertices[1]+vertices[2];
		//va[2].normalize();
		if (outTexturePos)
		{
			ta[0]=(texturePos[0]+texturePos[1])*0.5;
			ta[1]=texturePos[1];
			ta[2]=(texturePos[1]+texturePos[2])*0.5;
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1);

		va[0]=vertices[0];
		//va[1]=vertices[1]+vertices[2];
		//va[1].normalize();
		va[1]=va[2];
		va[2]=vertices[2];
		if (outTexturePos)
		{
			ta[0]=texturePos[0];
			ta[1]=(texturePos[1]+texturePos[2])*0.5;
			ta[2]=texturePos[2];
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1, true, true, false);
		return;
	}
	if (cDiscontinuity1 && !cDiscontinuity2 && cDiscontinuity3)
	{
		va[0]=vertices[0];
		va[1]=vertices[0];va[1]+=vertices[1];
		va[1].normalize();
		va[2]=vertices[0];va[2]+=vertices[2];
		va[2].normalize();
		if (outTexturePos)
		{
			ta[0]=texturePos[0];
			ta[1]=(texturePos[0]+texturePos[1])*0.5;
			ta[2]=(texturePos[0]+texturePos[2])*0.5;
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1);

		//va[0]=vertices[0]+vertices[1];
		//va[0].normalize();
		va[0]=va[1];
		va[1]=vertices[2];
		//va[2]=vertices[0]+vertices[2];
		//va[2].normalize();
		if (outTexturePos)
		{
			ta[0]=(texturePos[0]+texturePos[1])*0.5;
			ta[1]=texturePos[2];
			ta[2]=(texturePos[0]+texturePos[2])*0.5;
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1);


		//va[0]=vertices[0]+vertices[1];
		//va[0].normalize();
		va[1]=vertices[1];
		va[2]=vertices[2];
		if (outTexturePos)
		{
			ta[0]=(texturePos[0]+texturePos[1])*0.5;
			ta[1]=texturePos[1];
			ta[2]=texturePos[2];
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1, true, false, true);

		return;
	}
	if (!cDiscontinuity1 && cDiscontinuity2 && cDiscontinuity3)
	{
		va[0]=vertices[0];
		va[1]=vertices[1];
		va[2]=vertices[1];va[2]+=vertices[2];
		va[2].normalize();
		if (outTexturePos)
		{
			ta[0]=texturePos[0];
			ta[1]=texturePos[1];
			ta[2]=(texturePos[1]+texturePos[2])*0.5;
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1, false, true, true);

		//va[0]=vertices[1]+vertices[2];
		//va[0].normalize();
		va[0]=va[2];
		va[1]=vertices[2];
		va[2]=vertices[0];va[2]+=vertices[2];
		va[2].normalize();
		if (outTexturePos)
		{
			ta[0]=(texturePos[1]+texturePos[2])*0.5;
			ta[1]=texturePos[2];
			ta[2]=(texturePos[0]+texturePos[2])*0.5;
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1);

		va[1]=va[0];
		va[0]=vertices[0];
		//va[1]=vertices[1]+vertices[2];
		//va[1].normalize();
		//va[2]=vertices[0]+vertices[2];
		//va[2].normalize();
		if (outTexturePos)
		{
			ta[0]=texturePos[0];
			ta[1]=(texturePos[1]+texturePos[2])*0.5;
			ta[2]=(texturePos[0]+texturePos[2])*0.5;
		}
		projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1);
		return;
	}

	// Last case: the 3 sides have to be split: cut in 4 triangles a' la HTM
	va[0]=vertices[0];va[0]+=vertices[1];
	va[0].normalize();
	va[1]=vertices[1];va[1]+=vertices[2];
	va[1].normalize();
	va[2]=vertices[0];va[2]+=vertices[2];
	va[2].normalize();
	if (outTexturePos)
	{
		ta[0]=(texturePos[0]+texturePos[1])*0.5;
		ta[1]=(texturePos[1]+texturePos[2])*0.5;
		ta[2]=(texturePos[0]+texturePos[2])*0.5;
	}
	projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1);

	va[1]=va[0];
	va[0]=vertices[0];
	//va[1]=vertices[0]+vertices[1];
	//va[1].normalize();
	//va[2]=vertices[0]+vertices[2];
	//va[2].normalize();
	if (outTexturePos)
	{
		ta[0]=texturePos[0];
		ta[1]=(texturePos[0]+texturePos[1])*0.5;
		ta[2]=(texturePos[0]+texturePos[2])*0.5;
	}
	projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1);

	//va[0]=vertices[0]+vertices[1];
	//va[0].normalize();
	va[0]=va[1];
	va[1]=vertices[1];
	va[2]=vertices[1];va[2]+=vertices[2];
	va[2].normalize();
	if (outTexturePos)
	{
		ta[0]=(texturePos[0]+texturePos[1])*0.5;
		ta[1]=texturePos[1];
		ta[2]=(texturePos[1]+texturePos[2])*0.5;
	}
	projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1);

	va[0]=vertices[0];va[0]+=vertices[2];
	va[0].normalize();
	//va[1]=vertices[1]+vertices[2];
	//va[1].normalize();
	va[1]=va[2];
	va[2]=vertices[2];
	if (outTexturePos)
	{
		ta[0]=(texturePos[0]+texturePos[2])*0.5;
		ta[1]=(texturePos[1]+texturePos[2])*0.5;
		ta[2]=texturePos[2];
	}
	projectSphericalTriangle(clippingCap, va, outVertices, ta, outTexturePos, maxSqDistortion, nbI+1);

	return;
}

static QVarLengthArray<Vec3f, 4096> polygonVertexArray;
static QVarLengthArray<Vec2f, 4096> polygonTextureCoordArray;
static QVarLengthArray<unsigned int, 4096> indexArray;


// The function object that we use as an interface between VertexArray::foreachTriangle and
// StelPainter::projectSphericalTriangle.
//
// This is used by drawSphericalTriangles to project all the triangles coordinates in a StelVertexArray into our global
// vertex array buffer.
class VertexArrayProjector
{
public:
	VertexArrayProjector(const StelVertexArray& ar, StelPainter* apainter, const SphericalCap* aclippingCap,
						 QVarLengthArray<Vec3f, 4096>* aoutVertices, QVarLengthArray<Vec2f, 4096>* aoutTexturePos=NULL, double amaxSqDistortion=5.)
		   : vertexArray(ar), painter(apainter), clippingCap(aclippingCap), outVertices(aoutVertices),
			 outTexturePos(aoutTexturePos), maxSqDistortion(amaxSqDistortion)
	{
	}

	// Project a single triangle and add it into the output arrays
	inline void operator()(const Vec3d* v0, const Vec3d* v1, const Vec3d* v2,
						   const Vec2f* t0, const Vec2f* t1, const Vec2f* t2,
						   unsigned int, unsigned int, unsigned)
	{
		// XXX: we may optimize more by putting the declaration and the test outside of this method.
		const Vec3d tmpVertex[3] = {*v0, *v1, *v2};
		if (outTexturePos)
		{
			const Vec2f tmpTexture[3] = {*t0, *t1, *t2};
			painter->projectSphericalTriangle(clippingCap, tmpVertex, outVertices, tmpTexture, outTexturePos, maxSqDistortion);
		}
		else
			painter->projectSphericalTriangle(clippingCap, tmpVertex, outVertices, NULL, NULL, maxSqDistortion);
	}

	// Draw the resulting arrays
	void drawResult()
	{
		painter->setVertexPointer(3, GL_FLOAT, outVertices->constData());
		if (outTexturePos)
			painter->setTexCoordPointer(2, GL_FLOAT, outTexturePos->constData());
		painter->enableClientStates(true, outTexturePos != NULL);
		painter->drawFromArray(StelPainter::Triangles, outVertices->size(), 0, false);
		painter->enableClientStates(false);
	}

private:
	const StelVertexArray& vertexArray;
	StelPainter* painter;
	const SphericalCap* clippingCap;
	QVarLengthArray<Vec3f, 4096>* outVertices;
	QVarLengthArray<Vec2f, 4096>* outTexturePos;
	double maxSqDistortion;
};

void StelPainter::drawStelVertexArray(const StelVertexArray& arr, bool checkDiscontinuity)
{
	if (checkDiscontinuity && prj->hasDiscontinuity())
	{
		// The projection has discontinuities, so we need to make sure that no triangle is crossing them.
		drawStelVertexArray(arr.removeDiscontinuousTriangles(this->getProjector().data()), false);
		return;
	}

	setVertexPointer(3, GL_DOUBLE, arr.vertex.constData());
	if (arr.isTextured())
	{
		setTexCoordPointer(2, GL_FLOAT, arr.texCoords.constData());
		enableClientStates(true, true);
	}
	else
	{
		enableClientStates(true, false);
	}
	if (arr.isIndexed())
		drawFromArray((StelPainter::DrawingMode)arr.primitiveType, arr.indices.size(), 0, true, arr.indices.constData());
	else
		drawFromArray((StelPainter::DrawingMode)arr.primitiveType, arr.vertex.size());

	enableClientStates(false);
}

void StelPainter::drawSphericalTriangles(const StelVertexArray& va, bool textured,
                                         const SphericalCap* clippingCap,
                                         bool doSubDivide, double maxSqDistortion)
{
	if (va.vertex.isEmpty())
		return;

	Q_ASSERT(va.vertex.size()>2);
#ifndef STELPAINTER_GL2
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
	polygonVertexArray.clear();
	polygonTextureCoordArray.clear();
	indexArray.clear();

	if (!doSubDivide)
	{
		// The simplest case, we don't need to iterate through the triangles at all.
		drawStelVertexArray(va);
		return;
	}

	// the last case.  It is the slowest, it process the triangles one by one.
	{
		// Project all the triangles of the VertexArray into our buffer arrays.
		VertexArrayProjector result = va.foreachTriangle(VertexArrayProjector(va, this, clippingCap, &polygonVertexArray, textured ? &polygonTextureCoordArray : NULL, maxSqDistortion));
		result.drawResult();
		return;
	}
}

// Draw the given SphericalRegion.
void StelPainter::drawSphericalRegion(const SphericalRegion* poly, SphericalPolygonDrawMode drawMode, const SphericalCap* clippingCap, bool doSubDivise, double maxSqDistortion)
{
	if (!prj->getBoundingCap().intersects(poly->getBoundingCap()))
		return;

	switch (drawMode)
	{
		case SphericalPolygonDrawModeBoundary:
			Q_ASSERT_X(false, Q_FUNC_INFO,
			           "GL-REFACTOR - TODO boundary draw mode based on StelCircleArcRenderer");
			break;
		case SphericalPolygonDrawModeFill:
		case SphericalPolygonDrawModeTextureFill:
			Q_ASSERT_X(false, Q_FUNC_INFO,
			           "GL-REFACTOR - fill draw modes were refactored already");
		default:
			Q_ASSERT(0);
	}
}

void StelPainter::drawSprite2dMode(float x, float y, float radius)
{
	static float vertexData[] = {-10.,-10.,10.,-10., 10.,10., -10.,10.};
	static const float texCoordData[] = {0.,0., 1.,0., 0.,1., 1.,1.};
	vertexData[0]=x-radius; vertexData[1]=y-radius;
	vertexData[2]=x+radius; vertexData[3]=y-radius;
	vertexData[4]=x-radius; vertexData[5]=y+radius;
	vertexData[6]=x+radius; vertexData[7]=y+radius;
	enableClientStates(true, true);
	setVertexPointer(2, GL_FLOAT, vertexData);
	setTexCoordPointer(2, GL_FLOAT, texCoordData);
	drawFromArray(TriangleStrip, 4, 0, false);
	enableClientStates(false);
}

void StelPainter::drawSprite2dMode(const Vec3d& v, float radius)
{
	Vec3d win;
	prj->project(v, win);
	drawSprite2dMode(win[0], win[1], radius);
}

void StelPainter::drawSprite2dMode(float x, float y, float radius, float rotation)
{
	static float vertexData[8];
	static const float texCoordData[] = {0.,0., 1.,0., 0.,1., 1.,1.};

	// compute the vertex coordinates applying the translation and the rotation
	static const float vertexBase[] = {-1., -1., 1., -1., -1., 1., 1., 1.};
	const float cosr = std::cos(rotation / 180 * M_PI);
	const float sinr = std::sin(rotation / 180 * M_PI);
	for (int i = 0; i < 8; i+=2)
	{
		vertexData[i] = x + radius * vertexBase[i] * cosr - radius * vertexBase[i+1] * sinr;
		vertexData[i+1] = y + radius * vertexBase[i] * sinr + radius * vertexBase[i+1] * cosr;
	}

	enableClientStates(true, true);
	setVertexPointer(2, GL_FLOAT, vertexData);
	setTexCoordPointer(2, GL_FLOAT, texCoordData);
	drawFromArray(TriangleStrip, 4, 0, false);
	enableClientStates(false);
}

void StelPainter::drawRect2d(float x, float y, float width, float height, bool textured)
{
	static float vertexData[] = {-10.,-10.,10.,-10., 10.,10., -10.,10.};
	static const float texCoordData[] = {0.,0., 1.,0., 0.,1., 1.,1.};
	vertexData[0]=x; vertexData[1]=y;
	vertexData[2]=x+width; vertexData[3]=y;
	vertexData[4]=x; vertexData[5]=y+height;
	vertexData[6]=x+width; vertexData[7]=y+height;
	if (textured)
	{
		enableClientStates(true, true);
		setVertexPointer(2, GL_FLOAT, vertexData);
		setTexCoordPointer(2, GL_FLOAT, texCoordData);
	}
	else
	{
		enableClientStates(true);
		setVertexPointer(2, GL_FLOAT, vertexData);
	}
	drawFromArray(TriangleStrip, 4, 0, false);
	enableClientStates(false);
}

/*************************************************************************
 Draw a GL_POINT at the given position
*************************************************************************/
void StelPainter::drawPoint2d(float x, float y)
{
	static float vertexData[] = {0.,0.};
	vertexData[0]=x;
	vertexData[1]=y;

	enableClientStates(true);
	setVertexPointer(2, GL_FLOAT, vertexData);
	drawFromArray(Points, 1, 0, false);
	enableClientStates(false);
}


/*************************************************************************
 Draw a line between the 2 points.
*************************************************************************/
void StelPainter::drawLine2d(float x1, float y1, float x2, float y2)
{
	static float vertexData[] = {0.,0.,0.,0.};
	vertexData[0]=x1;
	vertexData[1]=y1;
	vertexData[2]=x2;
	vertexData[3]=y2;

	enableClientStates(true);
	setVertexPointer(2, GL_FLOAT, vertexData);
	drawFromArray(Lines, 2, 0, false);
	enableClientStates(false);
}

void StelPainter::setPointSize(qreal size)
{
#ifndef STELPAINTER_GL2
	glPointSize(size);
#else
	Q_UNUSED(size);
#endif
}

void StelPainter::setShadeModel(ShadeModel m)
{
#ifndef STELPAINTER_GL2
	glShadeModel(m);
#else
	Q_UNUSED(m);
#endif
}

void StelPainter::enableTexture2d(bool b)
{
#ifndef STELPAINTER_GL2
	if (b)
		glEnable(GL_TEXTURE_2D);
	else
		glDisable(GL_TEXTURE_2D);
#else
	texture2dEnabled = b;
#endif
}

//GL-REFACTOR: This has been refactored into QGL2Renderer and will be removed after 
//the rest of StelPainter is refactored.
void StelPainter::initSystemGLInfo(QGLContext* ctx)
{
	
	Q_ASSERT(glContext==NULL);
	glContext = ctx;

	makeMainGLContextCurrent();
	isNoPowerOfTwoAllowed = QGLFormat::openGLVersionFlags().testFlag(QGLFormat::OpenGL_Version_2_0) || QGLFormat::openGLVersionFlags().testFlag(QGLFormat::OpenGL_ES_Version_2_0);
}

#ifdef STELPAINTER_GL2
void StelPainter::TEMPSpecifyShaders(QGLShaderProgram *plain,
                                     QGLShaderProgram *color,
                                     QGLShaderProgram *texture,
                                     QGLShaderProgram *colorTexture)
{
	basicShaderProgram = plain;
	basicShaderVars.projectionMatrix = basicShaderProgram->uniformLocation("projectionMatrix");
	basicShaderVars.globalColor = basicShaderProgram->uniformLocation("globalColor");
	basicShaderVars.vertex = basicShaderProgram->attributeLocation("vertex");

	colorShaderProgram = color;

	texturesShaderProgram = texture;
	texturesShaderVars.projectionMatrix = texturesShaderProgram->
	                                      uniformLocation("projectionMatrix");
	texturesShaderVars.texCoord = texturesShaderProgram->attributeLocation("texCoord");
	texturesShaderVars.vertex = texturesShaderProgram->attributeLocation("vertex");
	texturesShaderVars.globalColor = texturesShaderProgram->uniformLocation("globalColor");
	texturesShaderVars.texture = texturesShaderProgram->uniformLocation("tex");
	
	texturesColorShaderProgram = colorTexture;
	texturesColorShaderVars.projectionMatrix = texturesColorShaderProgram->
	                                           uniformLocation("projectionMatrix");
	texturesColorShaderVars.texCoord = texturesColorShaderProgram->
	                                   attributeLocation("texCoord");
	texturesColorShaderVars.vertex = texturesColorShaderProgram->attributeLocation("vertex");
	texturesColorShaderVars.color = texturesColorShaderProgram->attributeLocation("color");
	texturesColorShaderVars.texture = texturesColorShaderProgram->uniformLocation("tex");
}
#endif


void StelPainter::setArrays(const Vec3d* vertice, const Vec2f* texCoords, const Vec3f* colorArray, const Vec3f* normalArray)
{
	enableClientStates(vertice, texCoords, colorArray, normalArray);
	setVertexPointer(3, GL_DOUBLE, vertice);
	setTexCoordPointer(2, GL_FLOAT, texCoords);
	setColorPointer(3, GL_FLOAT, colorArray);
	setNormalPointer(GL_FLOAT, normalArray);
}

void StelPainter::enableClientStates(bool vertex, bool texture, bool color, bool normal)
{
	vertexArray.enabled = vertex;
	texCoordArray.enabled = texture;
	colorArray.enabled = color;
	normalArray.enabled = normal;
}

void StelPainter::drawFromArray(DrawingMode mode, int count, int offset, bool doProj, const unsigned int* indices)
{
	ArrayDesc projectedVertexArray = vertexArray;
	if (doProj)
	{
		// Project the vertex array using current projection
		if (indices)
			projectedVertexArray = projectArray(vertexArray, 0, count, indices + offset);
		else
			projectedVertexArray = projectArray(vertexArray, offset, count, NULL);
	}

#ifndef STELPAINTER_GL2
	// Enable the client state and set the opengl array for each array
	Q_ASSERT(projectedVertexArray.enabled);
	Q_ASSERT(projectedVertexArray.pointer);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(projectedVertexArray.size, projectedVertexArray.type, 0, projectedVertexArray.pointer);
	if (texCoordArray.enabled)
	{
		Q_ASSERT(texCoordArray.pointer);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(texCoordArray.size, texCoordArray.type, 0, texCoordArray.pointer);
	}
	if (normalArray.enabled)
	{
		Q_ASSERT(normalArray.pointer);
		glEnableClientState(GL_NORMAL_ARRAY);
		glNormalPointer(normalArray.type, 0, normalArray.pointer);
	}
	if (colorArray.enabled)
	{
		Q_ASSERT(colorArray.pointer);
		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(colorArray.size, colorArray.type, 0, colorArray.pointer);
	}

#else
	QGLShaderProgram* pr=NULL;

	const Mat4f& m = getProjector()->getProjectionMatrix();
	const QMatrix4x4 qMat(m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]);

	if (!texCoordArray.enabled && !colorArray.enabled && !normalArray.enabled)
	{
		pr = basicShaderProgram;
		pr->bind();
		pr->setAttributeArray(basicShaderVars.vertex, (const GLfloat*)projectedVertexArray.pointer, projectedVertexArray.size);
		pr->enableAttributeArray(basicShaderVars.vertex);
		pr->setUniformValue(basicShaderVars.projectionMatrix, qMat);
		pr->setUniformValue(basicShaderVars.globalColor, currentColor[0], currentColor[1], currentColor[2], currentColor[3]);
	}
	else if (texCoordArray.enabled && !colorArray.enabled && !normalArray.enabled)
	{
		pr = texturesShaderProgram;
		pr->bind();
		pr->setAttributeArray(texturesShaderVars.vertex, (const GLfloat*)projectedVertexArray.pointer, projectedVertexArray.size);
		pr->enableAttributeArray(texturesShaderVars.vertex);
		pr->setUniformValue(texturesShaderVars.projectionMatrix, qMat);
		pr->setUniformValue(texturesShaderVars.globalColor, currentColor[0], currentColor[1], currentColor[2], currentColor[3]);
		pr->setAttributeArray(texturesShaderVars.texCoord, (const GLfloat*)texCoordArray.pointer, 2);
		pr->enableAttributeArray(texturesShaderVars.texCoord);
		//pr->setUniformValue(texturesShaderVars.texture, 0);    // use texture unit 0
	}
	else if (texCoordArray.enabled && colorArray.enabled && !normalArray.enabled)
	{
		pr = texturesColorShaderProgram;
		pr->bind();
		pr->setAttributeArray(texturesColorShaderVars.vertex, (const GLfloat*)projectedVertexArray.pointer, projectedVertexArray.size);
		pr->enableAttributeArray(texturesColorShaderVars.vertex);
		pr->setUniformValue(texturesColorShaderVars.projectionMatrix, qMat);
		pr->setAttributeArray(texturesColorShaderVars.texCoord, (const GLfloat*)texCoordArray.pointer, 2);
		pr->enableAttributeArray(texturesColorShaderVars.texCoord);
		pr->setAttributeArray(texturesColorShaderVars.color, (const GLfloat*)colorArray.pointer, colorArray.size);
		pr->enableAttributeArray(texturesColorShaderVars.color);
		//pr->setUniformValue(texturesShaderVars.texture, 0);    // use texture unit 0
	}
	else
	{
		qDebug() << "Unhandled parameters." << texCoordArray.enabled << colorArray.enabled << normalArray.enabled;
		qDebug() << "Light: " << light.isEnabled();
		return;
	}
#endif
	if (indices)
		glDrawElements(mode, count, GL_UNSIGNED_INT, indices + offset);
	else
		glDrawArrays(mode, offset, count);
#ifndef STELPAINTER_GL2
	glDisableClientState(GL_VERTEX_ARRAY);
	if (texCoordArray.enabled)
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if (normalArray.enabled)
		glDisableClientState(GL_NORMAL_ARRAY);
	if (colorArray.enabled)
		glDisableClientState(GL_COLOR_ARRAY);
#else
	if (pr==texturesColorShaderProgram)
	{
		pr->disableAttributeArray(texturesColorShaderVars.texCoord);
		pr->disableAttributeArray(texturesColorShaderVars.vertex);
		pr->disableAttributeArray(texturesColorShaderVars.color);
	}
	else if (pr==texturesShaderProgram)
	{
		pr->disableAttributeArray(texturesShaderVars.texCoord);
		pr->disableAttributeArray(texturesShaderVars.vertex);
	}
	else if (pr == texturesShaderProgram)
	{
		pr->disableAttributeArray(basicShaderVars.vertex);
	}
	if (pr)
		pr->release();
#endif
}


StelPainter::ArrayDesc StelPainter::projectArray(const StelPainter::ArrayDesc& array, int offset, int count, const unsigned int* indices)
{
	// XXX: we should use a more generic way to test whether or not to do the projection.
	if (dynamic_cast<StelProjector2d*>(prj.data()))
	{
		return array;
	}

	Q_ASSERT(array.size == 3);
	Q_ASSERT(array.type == GL_DOUBLE);
	Vec3d* vecArray = (Vec3d*)array.pointer;

	// We have two different cases :
	// 1) We are not using an indice array.  In that case the size of the array is known
	// 2) We are using an indice array.  In that case we have to find the max value by iterating through the indices.
	if (!indices)
	{
		polygonVertexArray.resize(offset + count);
		prj->project(count, vecArray + offset, polygonVertexArray.data() + offset);
	} else
	{
		// we need to find the max value of the indices !
		unsigned int max = 0;
		for (int i = offset; i < offset + count; ++i)
		{
			max = std::max(max, indices[i]);
		}
		polygonVertexArray.resize(max+1);
		prj->project(max + 1, vecArray + offset, polygonVertexArray.data() + offset);
	}

	ArrayDesc ret;
	ret.size = 3;
	ret.type = GL_FLOAT;
	ret.pointer = polygonVertexArray.constData();
	ret.enabled = array.enabled;
	return ret;
}

// Light methods

void StelPainterLight::setPosition(const Vec4f& v)
{
	position = v;
}

void StelPainterLight::setDiffuse(const Vec4f& v)
{
	diffuse = v;
}

void StelPainterLight::setSpecular(const Vec4f& v)
{
	specular = v;
}

void StelPainterLight::setAmbient(const Vec4f& v)
{
	ambient = v;
}

void StelPainterLight::setEnable(bool v)
{
	if (v)
		enable();
	else
		disable();
}

void StelPainterLight::enable()
{
	enabled = true;
}

void StelPainterLight::disable()
{
	enabled = false;
}


// material functions
StelPainterMaterial::StelPainterMaterial()
	: specular(0, 0, 0, 1), ambient(0.2, 0.2, 0.2, 1.0), emission(0, 0, 0, 1), shininess(0)
{
}

void StelPainterMaterial::setSpecular(const Vec4f& v)
{
	specular = v;
}

void StelPainterMaterial::setAmbient(const Vec4f& v)
{
	ambient = v;
}

void StelPainterMaterial::setEmission(const Vec4f& v)
{
	emission = v;
}

void StelPainterMaterial::setShininess(float v)
{
	shininess = v;
}
