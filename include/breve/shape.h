
#ifndef _SHAPE_H
#define _SHAPE_H

/*****************************************************************************
 *                                                                           *
 * The breve Simulation Environment                                          *
 * Copyright (C) 2000, 2001, 2002, 2003 Jonathan Klein                       *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation; either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program; if not, write to the Free Software               *
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA *
 *****************************************************************************/

#include <ode/ode.h>
#include <vector>

#include "vector.h"
#include "texture.h"

class slRenderGL;
class slVertexBufferGL;
class slWorld;

enum shapeTypes {
	ST_MESH,
	ST_BOX,
	ST_SPHERE
};

class slMeshShape;

/**
 * \brief A struct containing rotation and location information.
 */

class slPosition {
	public:
		double 				rotation[ 3 ][ 3 ];
		slVector 			location;
		
		// double 				_transform[ 4 ][ 4 ];
};

/**
 * \brief A plane, specified by a normal and a vertex.
 */

#define slPlaneDistance(plane, point) ( \
	(plane)->normal.x * ((point)->x - (plane)->vertex.x) + \
	(plane)->normal.y * ((point)->y - (plane)->vertex.y) + \
	(plane)->normal.z * ((point)->z - (plane)->vertex.z) )

/**
 * \brief A header used when serializing shape data.
 */

struct slSerializedShapeHeader {
	double 						_density;

	slVector 					_max;
	int 						_type; 
	double 						_radius;
    
	int 						_faceCount;
};  

typedef struct slSerializedShapeHeader slSerializedShapeHeader;

class slCamera;

class slPoint;
class slFace;
class slEdge;

class slFeature {
	public:
		slFeature() {
			voronoi = NULL;
		}
		virtual ~slFeature() {
			if( voronoi )
		    	delete[] voronoi;
		}

		int type;
		slPlane *voronoi;	
};

/*!
	\brief A point on a shape.
*/

class slPoint : public slFeature {
	public:
		slPoint() : slFeature() {} 

		slVector 						vertex;
};

/*!
	\brief An edge on a shape.

	The endpoints of the edge are the vertex neighbors 0 and 1.
*/

class slEdge : public slFeature {
	public:
		slFeature *neighbors[4];
		slFace *faces[2];
		slPoint *points[2];

		slEdge() {
			voronoi = new slPlane[4];
		}

		~slEdge() {
		}
};

/*!
	\brief A face on a shape.
*/

class slFace : public slFeature {
	public:
		int						_pointCount;

		slPlane 				plane;

		slEdge**				neighbors;		// neighbor edges
		slPoint**				points;			// connected points 
		slFace**				faces;			// connected faces

		char 					drawFlags;

		~slFace() {
			delete[] neighbors;
			delete[] faces;
			delete[] points;
		}
};


/**
 * A shape in the simulated world.
 */

class slShape {
	friend class 					slWorldObject;
	friend class 					slVclipData;

	public:
		slShape() {
			_type 							= ST_MESH;
			_density 						= 1.0;
			_referenceCount 				= 1;

			_odeGeomID[ 0 ]					= 0;
			_odeGeomID[ 1 ]					= 0;

			slMatrixIdentity( _transform );
			slVectorSet( &_scale, 1, 1, 1 );
		}

		int									findPointIndex( slVector *inVertex );

		static void 						slMatrixToODEMatrix( const double inM[ 3 ][ 3 ], dReal *outM );

		void 								retain();
		void 								release();

		virtual void 						updateLastPosition( slPosition *inPosition ) {}

		virtual ~slShape();

		virtual void 						bounds( const slPosition *position, slVector *min, slVector *max ) const;
		virtual int 						pointOnShape(slVector *dir, slVector *point);
		virtual int 						rayHitsShape(slVector *dir, slVector *target, slVector *point);

		void 								setScale( slVector *inSize );
		const slVector* 					scale() const { return &_scale; };

		virtual slSerializedShapeHeader* 	serialize(int *length);

		static slShape* 					deserialize( slSerializedShapeHeader* inData );

		virtual void 						drawShadowVolume(slWorld *w, slCamera *camera, slPosition *position);

		virtual void 						draw( const slRenderGL& inRender );

		virtual void 						setMass(double mass);
		virtual void 						setDensity(double density);
		virtual void 						setInertiaMatrix( slMatrix inInertia );
		virtual void						finishShape( double inDensity );

		double 								getMass();
		double 								getDensity();


		double 								_inertia[3][3];
		double 								_mass;
		double 								_density;

		// the max reach on each axis 

		int 								_type;

		std::vector< slFeature* > 			features;

		std::vector< slFace* > 				faces;
		std::vector< slEdge* > 				edges;
		std::vector< slPoint* > 			points;

		dGeomID								_odeGeomID[ 2 ];

	protected:
		slMatrix							_transform;
		slVector							_scale;

		virtual void 						fillVertexBuffer();

		slVector 							_max, _min;
		int 								_referenceCount;
		slVertexBufferGL					_vertexBuffer;

};

class slBox : public slShape {
	public:	
											slBox( slVector *inSize, double inDensity );
		slSerializedShapeHeader* 			serialize(int *length);

		virtual void						finishShape( double inDensity );


	protected:
		slVector							_size;
};

class slSphere : public slShape {
	public:
		/** \brief Creates a new sphere of a given radius and density. */
							slSphere(double radius, double density);

		void 					bounds( const slPosition *position, slVector *min, slVector *max ) const;
		int 					pointOnShape(slVector *dir, slVector *point);
		int 					rayHitsShape(slVector *dir, slVector *target, slVector *point);
		void 					scale(slVector *point);

		virtual 				void setDensity(double density);

		slSerializedShapeHeader*		serialize(int *length);

		void 					drawShadowVolume(slWorld *w, slCamera *camera, slPosition *position);

		float					radius() const { return _radius; } 

	protected:
		virtual void 				fillVertexBuffer();

		int 				addTriangles( GLfloat *inX, GLfloat *inY, GLfloat *inZ, int inDiv, float inR, int inIndex );
		float				_radius;
};

/*!
 * A header used when serializing face data.  
 * It's only contains an integer at the moment.
 */

struct slSerializedFaceHeader {
	int vertexCount;
};

typedef struct slSerializedFaceHeader slSerializedFaceHeader;

/*!
 * \brief Transforms a vector with the given position.
 */

slVector *slPositionVertex( const slPosition *p, const slVector *v, slVector *o );

slShape *slNewNGonDisc(int count, double radius, double height, double density);
slShape *slNewNGonCone(int count, double radius, double height, double density);

slFace *slAddFace(slShape *v, slVector **points, int vCount);
slEdge *slAddEdge(slShape *s, slFace *theFace, slVector *start, slVector *end);

slPoint *slAddPoint(slShape *v, slVector *start);

slShape *slSetCube(slShape *s, slVector *a, double density);

slShape *slSetNGonDisc( slMeshShape *s, int sideCount, double radius, double height, double density);
slShape *slSetNGonCone( slMeshShape *s, int sideCount, double radius, double height, double density);

slPlane *slSetPlane(slPlane *p, slVector *normal, slVector *vertex);
void slSphereInertiaMatrix(double radius, double mass, double i[3][3]);

int slRayHitsShape(slShape *s, slVector *dir, slVector *target, slVector *point);

#endif // SHAPE_H
