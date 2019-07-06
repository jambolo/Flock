#if !defined( BOID_H_INCLUDED )
#define BOID_H_INCLUDED

#pragma once

/*****************************************************************************

                                    Boid.h

						Copyright 2001, John J. Bolton
	----------------------------------------------------------------------

	$Header: //depot/Flock/Boid.h#2 $

	$NoKeywords: $

*****************************************************************************/


#include <vector>
#include "Math/Vector3f.h"
#include "Math/Range.h"

class HeightField;


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/


class Boid;

class BoidList : public std::vector< Boid * > {};


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

class Boid
{
public:

	Boid( Vector3f const & position, Vector3f const & velocity );
	virtual ~Boid();

	void Update( float dt,
				 BoidList const & boids,
				 HeightField const & terrain, float xyScale,
				 float seaLevel );

	Vector3f	m_Position;
	Vector3f	m_Velocity;

	static float const					MAX_ACCELERATION;
	static float const					MAX_SPEED_XY;
	static float const					MAX_SPEED_Z;
	static float const					DESIRED_SPEED;
//	static Math::Range< float > const	DESIRED_SEPARATION;
	static float const					DESIRED_SEPARATION;
	static Math::Range< float > const	DESIRED_HEIGHT;
	static float const					DESIRED_WATER_DISTANCE;
	static float const					MAX_PERCEPTION_DISTANCE;		

private:

	// Return the closest boid in the list
	Boid const *	FindClosest( BoidList const & boids ) const;

	// Return the change in velocity for unaffected movement
	Vector3f	Cruise() const;

	// Return the change in velocity to avoid something
	Vector3f	AvoidTerrain( HeightField const & terrain, float xyScale, float seaLevel ) const;

	// Return the change in velocity to avoid something
	bool		OverWater( HeightField const & terrain, float xyScale, float seaLevel ) const;

	// Return the change in velocity to achieve the desired separation
	Vector3f	Separate( BoidList const & boids ) const;

	// Compute the change in velocity to be aligned with nearby boids
	Vector3f	Align( BoidList const & boids ) const;

	// Compute the change in velocity to achieve the desired closeness to nearby boids
	Vector3f	Congregate( BoidList const & boids ) const;

	void		Wrap( HeightField const & terrain, float xyScale );

};

#endif // !defined( BOID_H_INCLUDED )
