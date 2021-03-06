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

int gPhysicsError;
const char *gPhysicsErrorMessage;

#include "render.h"
#include "simulation.h"
#include "world.h"
#include "tiger.h"
#include "link.h"
#include "multibody.h"
#include "integrate.h"
#include "vclip.h"
#include "vclipData.h"
#include "gldraw.h"

void slODEErrorHandler( int errnum, const char *msg, va_list ap ) {
	static char error[ 2048 ];

	vsnprintf( error, 2047, msg, ap );

	gPhysicsErrorMessage = error;

	slMessage( DEBUG_WARN, "ODE physics engine message: %s\n", error );
}

/**
 * Creates a new empty world.
 */

slWorld::slWorld() {
	gPhysicsError = 0;
	gPhysicsErrorMessage = NULL;

	slAllocIntegrationVectors( this );

	_odeWorldID = dWorldCreate();
	dWorldSetQuickStepNumIterations( _odeWorldID, 60 );

	dWorldSetCFM( _odeWorldID, 1e-6 );
	dWorldSetERP( _odeWorldID, 0.1 );
	dSetErrorHandler( slODEErrorHandler );
	dSetMessageHandler( slODEErrorHandler );

	dInitODE();

	_odeCollisionGroupID = dJointGroupCreate( 0 );

	_resolveCollisions = 0;
	_detectCollisions = 0;

	_initialized = 0;

	_age = 0.0;

	_detectCollisions = 1;
	_detectLightExposure = 0;
	_drawLightExposure = 0;

	_collisionCallback = NULL;
	_collisionCheckCallback = NULL;

	integrator = slEuler;

	_boundingBoxOnlyCollisions = 0;

	_odeStepMode = 0;

	backgroundTexture = NULL;

	slVectorSet( &backgroundTextureColor, 1, 1, 1 );

	slVectorSet( &_lightExposureCamera._target, 0, 0, 0 );

	_proximityData = NULL;
	_clipGrid = NULL;
	_clipData = new slVclipData();

	slVector g;
	slVectorSet( &g, 0.0, -9.81, 0.0 );

	setGravity( &g );

	_lights[ 0 ]._type = LightInfinite;
	slVectorSet( &_lights[0]._location, 0, 0, 0 );
	slVectorSet( &_lights[0]._ambient, .2, .2, .2 );
	slVectorSet( &_lights[0]._diffuse, .6, .9, .9 );
	slVectorSet( &_lights[0]._specular, 1, 1, 1 );
}

/**
 *	Startup a netsim server.
 */

int slWorld::startNetsimServer() {
#if HAVE_LIBENET
	enet_initialize();

	_netsimData._isMaster = 1;

	_netsimData._server = new slNetsimServer( this );

	if ( !_netsimData._server )
		return -1;

	_netsimData._server->start();

	return 0;

#else
	slMessage( DEBUG_ALL, "error: cannot start netsim server -- not compiled with enet support\n" );
	return -1;

#endif
}

/**
 * \brief Startup as a netsim slave.
 */

int slWorld::startNetsimSlave( const char *host ) {
#if HAVE_LIBENET
	enet_initialize();

	_netsimData._isMaster = 0;

	_netsimData._server = new slNetsimServer( this );

	_netsimClient = _netsimData._server->openConnection( host, NETSIM_MASTER_PORT );

	if ( !_netsimData._server )
		return -1;

	_netsimData._server->start();

	return 0;

#else
	slMessage( DEBUG_ALL, "error: cannot start netsim slave -- not compiled with enet support\n" );
	return -1;
#endif
}

/**
 * \brief frees an slWorld object, including all of its objects and its clipData.
 */

slWorld::~slWorld() {
	std::vector<slWorldObject*>::iterator wi;
	std::vector<slPatchGrid*>::iterator pi;

	for ( wi = _objects.begin(); wi != _objects.end(); wi++ ) delete *wi;

	for ( pi = _patches.begin(); pi != _patches.end(); pi++ ) delete *pi;

	if ( _clipData ) delete _clipData;

	if ( _proximityData ) delete _proximityData;

	dWorldDestroy( _odeWorldID );

	dJointGroupDestroy( _odeCollisionGroupID );

#if HAS_LIBENET
	if ( _netsimData._server ) {
		delete _netsimData._server;
		enet_deinitialize();
	}
#endif

	slFreeIntegrationVectors( this );

	dCloseODE();
}


void slWorld::draw( slRenderGL& inRenderer, slCamera *inCamera ) {
	float c[] = { backgroundColor.x, backgroundColor.y, backgroundColor.z, 1.0 };
	inRenderer.Clear( c );
	inRenderer.SetBlendMode( slBlendAlpha );


	if( inCamera -> _drawBlur ) {
		slVector center, a1, a2; 
		center.set( 0, 0, 0 ); 
		a1.set( 1, 0, 0 ); 
		a2.set( 0, 1, 0 );
		
		glDisable( GL_CULL_FACE );
		
		float c[4] = { 1, 1, 1, 1 };
		inRenderer.SetBlendColor( c );

		inRenderer.SetDepthWriteEnabled( false );
		inRenderer.SetIdentity( slMatrixProjection );
		inRenderer.SetIdentity( slMatrixGeometry );
		inRenderer.SetIdentity( slMatrixTexture );
		inRenderer.DrawQuad( *inCamera -> readbackTexture(), center, a1, a2 );
		inRenderer.SetDepthWriteEnabled( true );
	}
	
	inRenderer.ApplyCamera( inCamera );

	_skybox.draw( inRenderer, inCamera );
	
	drawPatchGrids( inRenderer, inCamera );

	if( inCamera -> _drawLights && !inCamera -> _drawShadowVolumes )
		inRenderer.PushLights(_lights);

	drawObjects( inRenderer );

	if( inCamera -> _drawLights && !inCamera -> _drawShadowVolumes )
		inRenderer.PopLights(_lights);

	inCamera -> processBillboards( this );
	inCamera -> renderBillboards( inRenderer );	
	
	if( inCamera -> _drawShadow && inCamera -> _shadowCatcher ) {

		inRenderer.BeginFlatShadows( inCamera, &_lights[ 0 ]._location );

		for ( unsigned int n = 0; n < _objects.size(); n++ ) {
			slWorldObject *wo = _objects[ n ];
			
			if( wo && wo != inCamera -> _shadowCatcher )
				wo -> draw( inRenderer );
		}
		
		inRenderer.EndFlatShadows();
	}
	
	if( inCamera -> _drawShadowVolumes )
		renderShadowVolume(inRenderer, inCamera);

	if( inCamera -> _drawBlur ) 
		inRenderer.ReadToTexture( *inCamera );
}

void slWorld::drawPatchGrids( slRenderGL& inRenderer, slCamera *inCamera ) {
	for ( unsigned int n = 0; n < _patches.size(); n++ ) {
		slPatchGrid *pg = _patches[ n ];
		
		if( pg )
			pg -> draw( inRenderer, inCamera );
	}
}

void slWorld::drawObjects( slRenderGL& inRenderer ) {
	for ( unsigned int n = 0; n < _objects.size(); n++ ) {
		slWorldObject *wo = _objects[ n ];
		
		if( wo && wo -> _textureMode == slBitmapNone )
			wo -> draw( inRenderer );
	}
}






/**
 * \brief Adds a camera to the world.
 */

void slWorld::addCamera( slCamera *camera ) {
	_cameras.push_back( camera );
}

/**
 * \brief Removes a camera from the world.
 */

void slWorld::removeCamera( slCamera *camera ) {
	std::vector<slCamera*>::iterator ci;

	ci = std::find( _cameras.begin(), _cameras.end(), camera );

	_cameras.erase( ci );
}

/**
 * \brief Renders all of the cameras in the world .
 */

//void slWorld::renderCameras() {
//	std::vector<slCamera*>::iterator ci;
//
//	for ( ci = _cameras.begin(); ci != _cameras.end(); ci++ )
//		( *ci )->renderWorld( this, 0, 1 );
//}

/**
 * Adds an object to the world.
 */

void slWorld::addObject( slWorldObject *inObject ) {
	_objects.push_back( inObject );
	_initialized = false;
}

/**
	\brief Removes an object from the world.
*/

void slWorld::removeObject( slWorldObject *p ) {
	std::vector<slWorldObject*>::iterator wi;

	wi = std::find( _objects.begin(), _objects.end(), p );

	if ( wi != _objects.end() ) 
		*wi = NULL;
}

void slWorld::removeEmptyObjects( ) {
	for( unsigned int i = 0; i < _objects.size(); i++ ) {
		// UGLY!

		if( _objects[ i ] == NULL ) {
			_objects.erase( _objects.begin() + i );
			_initialized = false;
			i--;
		}
	}
}

slPatchGrid *slWorld::addPatchGrid( slVector *center, slVector *patchSize, int x, int y, int z ) {

	slPatchGrid *g = new slPatchGrid( center, patchSize, x, y, z );

	_patches.push_back( g );

	return g;
}

void slWorld::removePatchGrid( slPatchGrid *g ) {
	std::vector<slPatchGrid*>::iterator pi;

	pi = std::find( _patches.begin(), _patches.end(), g );

	if ( pi != _patches.end() ) {
		delete *pi;
		_patches.erase( pi );
		_initialized = 0;
	}
}

/**
	\brief Creates a new stationary object.
*/

slStationary::slStationary( slShape *s, slVector *loc, double rot[3][3], void *data ) {
	_type = WO_STATIONARY;

	setShape( s );

	_userData = data;

	slVectorCopy( loc, &_position.location );
	slMatrixCopy( rot,  _position.rotation );
}

/**
	\brief Runs the world for deltaT seconds, in steps of the given size.

	Makes repeated calls to \ref slWorldStep to step the world forward
	deltaT seconds.
*/

double slWorld::runWorld( double deltaT, double timestep, int *error ) {
	double total = 0.0;

	removeEmptyObjects();

	*error = 0;

	if ( !_initialized ) slVclipDataInit( this );

	while ( total < deltaT && !*error ) {
		total += step( timestep, error );
	}

	_age += total;

	return total;
}

/**
	\brief Takes a single simulation step.
*/

double slWorld::step( double stepSize, int *error ) {
	unsigned int simulate = 0;
	std::vector<slWorldObject*>::iterator wi;
	std::vector<slObjectConnection*>::iterator li;
	int result;

	for ( wi = _objects.begin(); wi != _objects.end(); wi++ ) {
		simulate += ( *wi )->isSimulated();
		( *wi )->step( this, stepSize );
	}

	for ( li = _connections.begin(); li != _connections.end(); li++ )
		( *li )->step( stepSize );

	if ( _detectCollisions ) {
		
		if ( !_initialized ) 
			slVclipDataInit( this );

		if ( _clipGrid ) {
			_clipGrid->assignObjectsToPatches( this );
			result = 0;
		} else {
			_clipData -> pruneAndSweep();
			result = _clipData->clip( 0.0, 0, _boundingBoxOnlyCollisions );
		}

		if ( result == -1 ) {
			slMessage( DEBUG_ALL, "warning: error in vclip\n" );
			( *error )++;
			return 0;
		}

		for ( unsigned int cn = 0; cn < _clipData->collisionCount; cn++ ) {
			slCollision *c = &_clipData->collisions[ cn];
			slWorldObject *w1;
			slWorldObject *w2;
			slPairFlags *flags;

			w1 = _objects[ c->n1 ];
			w2 = _objects[ c->n2 ];

			flags = slVclipPairFlags( _clipData, c->n1, c->n2 );

			if ( w1 && w2 && ( *flags & BT_SIMULATE ) ) {
				dBodyID bodyX = NULL, bodyY = NULL;
				dJointID id;
				double mu = 0;
				double e = 0;
				double erp = 0;
				double cfm = 0;

				if ( w1->getType() == WO_LINK ) {
					slLink *l = ( slLink* )w1;
					bodyX = l->_odeBodyID;
				}

				if ( w2->getType() == WO_LINK ) {
					slLink *l = ( slLink* )w2;
					bodyY = l->_odeBodyID;
				}

				if ( w1->getType() == WO_LINK && w2->getType() == WO_LINK ) {
					slMultibody *mb1 = (( slLink* )w1 )->getMultibody();
					slMultibody *mb2 = (( slLink* )w2 )->getMultibody();

					if ( mb1 && mb1 == mb2 && mb1->getCFM() != 0.0 ) {
						cfm = mb1->getCFM();
						erp = mb1->getERP();
					}
				}

				mu = 1.0 + ( w1->_mu + w2->_mu ) / 2.0;

				// e = ( w1->_e + w2->_e ) / ( 2.0 * ( c -> _contactPoints ) );
				e = ( w1->_e + w2->_e ) / 2.0;

				for ( int n = 0; n < c -> _contactPoints; n++ ) {
					dContact contact;

					memset( &contact, 0, sizeof( dContact ) );
					memcpy( &contact.geom, &c -> _contactGeoms[ n ], sizeof( dContactGeom ) );

					contact.geom.g1 = NULL;
					contact.geom.g2 = NULL;

					contact.surface.mode = dContactBounce | dContactApprox1 | dContactSoftERP;
					contact.surface.soft_erp = 0.05;
					contact.surface.mu = mu;
					contact.surface.mu2 = 0;
					contact.surface.bounce = e;
					contact.surface.bounce_vel = 0.05;

					if( fabs( contact.geom.depth ) > 0.4 ) {
					//	printf( "depth = %f, e = %f\n", contact.geom.depth, e );
					//	printf( "normal: ( %f, %f, %f )\n", contact.geom.normal[ 0 ], contact.geom.normal[ 1 ], contact.geom.normal[ 2 ] );
					}

				
					if( cfm != 0.0 || erp != 0.0 ) {
						contact.surface.mode |= dContactSoftCFM;
						contact.surface.soft_cfm = cfm;
						contact.surface.soft_erp = erp;
					}

					id = dJointCreateContact( _odeWorldID, _odeCollisionGroupID, &contact );
					dJointAttach( id, bodyX, bodyY );
				}
			}

			if ( ( *flags & BT_CALLBACK ) && _collisionCallback && w1 && w2 ) {
				slVector pos, normal;

				dContactGeom *geom = &c -> _contactGeoms[ 0 ];

				slVectorSet( &pos,    geom -> pos[ 0 ],    geom -> pos[ 1 ],    geom -> pos[ 2 ] );
				slVectorSet( &normal, geom -> normal[ 0 ], geom -> normal[ 1 ], geom -> normal[ 2 ] );

				_collisionCallback( w1->getCallbackData(), w2->getCallbackData(), CC_NORMAL, &pos, &normal );
			}
		}
	}

	for ( wi = _objects.begin(); wi != _objects.end(); wi++ ) {
		slWorldObject *w = *wi;

		if( w && w -> _shape )
			w -> _shape -> updateLastPosition( &w -> _position );
	}

	if ( simulate != 0 ) {
		if ( _odeStepMode == 0 ) 
			dWorldStep( _odeWorldID, stepSize );
		else
			dWorldQuickStep( _odeWorldID, stepSize );

		dJointGroupEmpty( _odeCollisionGroupID );

		if ( gPhysicsError )( *error )++;
	}

	removeEmptyObjects();

	return stepSize;
}

void slWorld::updateNeighbors() {
	double dist;
	slVector *location, diff;
	slWorldObject *o1, *o2;
	std::vector<slWorldObject*>::iterator wi;

	if ( !_initialized ) slVclipDataInit( this );

	if ( !_proximityData ) {
		_proximityData = new slVclipData();
		_proximityData->realloc( _objects.size() );
		slInitProximityData( this );
		slInitBoundSort( _proximityData );
		_proximityData->clip( 0.0, 1, 0 );
	}

	// update all the bounding boxes first

	for ( wi = _objects.begin(); wi != _objects.end(); wi++ ) {
		slWorldObject *wo = *wi;

		if( wo ) {
			location = &wo->_position.location;

			wo->_neighborMin.x = location->x - wo->_proximityRadius;
			wo->_neighborMin.y = location->y - wo->_proximityRadius;
			wo->_neighborMin.z = location->z - wo->_proximityRadius;
			wo->_neighborMax.x = location->x + wo->_proximityRadius;
			wo->_neighborMax.y = location->y + wo->_proximityRadius;
			wo->_neighborMax.z = location->z + wo->_proximityRadius;
			wo->_neighbors.clear();
			wo->_neighborData.clear();
		}
	}

	// vclip, but stop after the pruning stage

	_proximityData->pruneAndSweep();

	std::map< slPairFlags* , slCollisionCandidate >::iterator ci;

	for ( ci = _proximityData->_candidates.begin(); ci != _proximityData->_candidates.end(); ci++ ) {
		slCollisionCandidate c = ci->second;

		o1 = _objects[ c._x ];
		o2 = _objects[ c._y ];

		if( o1 && o2 ) {
			slVectorSub( &o1->_position.location, &o2->_position.location, &diff );
			dist = slVectorLength( &diff );

			if ( dist < o1->_proximityRadius ) {
				o1->_neighbors.push_back( o2 );
				o1->_neighborData.push_back( o2->getCallbackData() );
			}

			if ( dist < o2->_proximityRadius ) {
				o2->_neighbors.push_back( o1 );
				o2->_neighborData.push_back( o1->getCallbackData() );
			}
		}
	}
}

void slWorld::setGravity( slVector *inG ) {
	dWorldSetGravity( _odeWorldID, inG -> x, inG -> y, inG -> z );
}

slObjectLine *slWorld::addObjectLine( slWorldObject *src, slWorldObject *dst, int stipple, slVector *color ) {
	slObjectLine *line;

	if ( src == dst ) return NULL;

	line = new slObjectLine;

	line->_stipple = stipple;

	line->_dst = dst;

	line->_src = src;

	slVectorCopy( color, &line->_color );

	addConnection( line );

	return line;
}

void slWorld::addConnection( slObjectConnection *c ) {
	_connections.push_back( c );
	c->_src->_connections.push_back( c );
	c->_dst->_connections.push_back( c );
}

void slWorld::removeConnection( slObjectConnection *c ) {
	std::vector<slObjectConnection*>::iterator li;

	if ( !c ) return;

	if ( c->_src ) {
		li = std::find( c->_src->_connections.begin(), c->_src->_connections.end(), c );

		if ( li != c->_src->_connections.end() ) c->_src->_connections.erase( li );
	}

	if ( c->_dst ) {
		li = std::find( c->_dst->_connections.begin(), c->_dst->_connections.end(), c );

		if ( li != c->_dst->_connections.end() ) c->_dst->_connections.erase( li );
	}

	li = std::find( _connections.begin(), _connections.end(), c );

	if ( li != _connections.end() ) _connections.erase( li );

	delete c;
}

/**
	\brief Returns the age of the world.
*/

double slWorld::getAge() {
	return _age;
}

/**
	\brief Sets the age of the world.
*/

void slWorld::setAge( double a ) {
	_age = a;
}

/**
	\brief Sets the collision detection structures as uninitialized.
*/

void slWorld::setUninitialized() {
	_initialized = 0;
}

/**
	\brief Sets collision resolution on or off.
*/

void slWorld::setCollisionResolution( bool n ) {
	_resolveCollisions = n;
}

void slWorld::setBoundsOnlyCollisionDetection( bool b ) {
	_boundingBoxOnlyCollisions = b;
}

void slWorld::setPhysicsMode( int n ) {
	_odeStepMode = n;
}

void slWorld::setBackgroundColor( slVector *v ) {
	slVectorCopy( v, &backgroundColor );
}

void slWorld::setBackgroundTextureColor( slVector *v ) {
	slVectorCopy( v, &backgroundTextureColor );
}

void slWorld::setBackgroundTexture( slTexture2D *inTexture ) {
	backgroundTexture = inTexture;
}


void slWorld::setCollisionCallbacks( int( *check )( void*, void*, int t ), void( *collide )( void*, void*, int t, slVector*, slVector* ) ) {
	_collisionCallback = collide;
	_collisionCheckCallback = check;
}

slWorldObject *slWorld::getObject( unsigned int n ) {
	return ( n >= _objects.size() ) ? NULL : _objects[ n ];
}

void slWorld::setQuickstepIterations( int n ) {
	dWorldSetQuickStepNumIterations( _odeWorldID, n );
}

void slWorld::setAutoDisableFlag( bool f ) {
	dWorldSetAutoDisableFlag( _odeWorldID, f );
}

