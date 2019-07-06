/*****************************************************************************

                                   Boid.cpp

						Copyright 2001, John J. Bolton
	----------------------------------------------------------------------

	$Header: //depot/Flock/Boid.cpp#2 $

	$NoKeywords: $

*****************************************************************************/

#include "Boid.h"

#include <vector>
#include <limits>
#include "Math/Vector3f.h"
#include "Math/Range.h"
#include "Heightfield/Heightfield.h"

float const					Boid::MAX_SPEED_XY						= 20.000f;
float const					Boid::MAX_SPEED_Z						= 10.000f;
float const					Boid::MAX_ACCELERATION					= 5.00f;
float const					Boid::DESIRED_SPEED						= 10.000f;
//Math::Range< float > const	Boid::DESIRED_SEPARATION			       	( 0.100f, .2000f );
float const					Boid::DESIRED_SEPARATION				= 1.000f;
Math::Range< float > const	Boid::DESIRED_HEIGHT					( 1.000f, 4.000f );
float const					Boid::DESIRED_WATER_DISTANCE			= 1.000f;
float const					Boid::MAX_PERCEPTION_DISTANCE			= 20.00f;		

/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

Boid::Boid( Vector3f const & position, Vector3f const & velocity )
	: m_Position( position ), m_Velocity( velocity )
{
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

Boid::~Boid()
{
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

void Boid::Update( float dt,
				   BoidList const & boids,
				   HeightField const & terrain, float xyScale,
				   float seaLevel )
{
	Vector3f	acceleration	= Vector3f::ORIGIN;

	acceleration += Cruise();
	acceleration += AvoidTerrain( terrain, xyScale, seaLevel );
	acceleration += Align( boids );
	acceleration += Congregate( boids );

	m_Velocity += acceleration;

	m_Position += m_Velocity * dt;

	Wrap( terrain, xyScale );

	// If the boid is over water, then put him back and reverse his velocity in
	// the XY plane and make him go back instead

	if ( OverWater( terrain, xyScale, seaLevel ) )
	{
		// Put him back
		
		m_Position -= m_Velocity * dt;

		// Reverse his direction

		m_Velocity.m_X = -m_Velocity.m_X;
		m_Velocity.m_Y = -m_Velocity.m_Y;

		// Go the other way

		m_Position += m_Velocity * dt;

		Wrap( terrain, xyScale );
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

void Boid::Wrap( HeightField const & terrain, float xyScale )
{
	float const	tw	= ( terrain.GetSizeX() - 1.f ) * xyScale;
	float const	tw2	= tw * .5f;

	if ( m_Position.m_X < -tw2 )
	{
		m_Position.m_X += tw;
	}
	else if ( m_Position.m_X > tw2 )
	{
		m_Position.m_X -= tw;
	}

	float const	th	= ( terrain.GetSizeY() - 1.f ) * xyScale;
	float const	th2	= th * .5f;

	if ( m_Position.m_Y < -th2 )
	{
		m_Position.m_Y += th;
	}
	else if ( m_Position.m_Y > th2 )
	{
		m_Position.m_Y -= th;
	}
}

/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

Vector3f	Boid::Cruise() const
{
	float const	currentSpeed	= m_Velocity.Length();

	if ( Math::IsCloseToZero( currentSpeed ) )
	{
		return Vector3f::Y_AXIS * DESIRED_SPEED;
	}
	else
	{
		return m_Velocity * ( DESIRED_SPEED / currentSpeed - 1.f );	// Optimization: was m_Velocity.Normalize() * DESIRED_SPEED - m_Velocity
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

Vector3f	Boid::AvoidTerrain( HeightField const & terrain, float xyScale, float seaLevel ) const
{
	int const	tx		= m_Position.m_X / xyScale + ( terrain.GetSizeX() - 1.f ) * .5f + .5f;
	int const	ty		= m_Position.m_Y / xyScale + ( terrain.GetSizeY() - 1.f ) * .5f + .5f;
	float const	height	= m_Position.m_Z - terrain.GetZ( tx, ty );

	if ( height < DESIRED_HEIGHT || m_Position.m_Z <= seaLevel )
	{
		return Vector3f::Z_AXIS * MAX_ACCELERATION;
	}
	else if ( height > DESIRED_HEIGHT )
	{
		return Vector3f::Z_AXIS * -MAX_ACCELERATION;
	}
	else
	{
		return Vector3f::ORIGIN;
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

bool	Boid::OverWater( HeightField const & terrain, float xyScale, float seaLevel ) const
{
	int const	tx		= m_Position.m_X / xyScale + ( terrain.GetSizeX() - 1.f ) * .5f + .5f;
	int const	ty		= m_Position.m_Y / xyScale + ( terrain.GetSizeY() - 1.f ) * .5f + .5f;
	float const	depth	= seaLevel - terrain.GetZ( tx, ty );

	return ( depth <= 0.f );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

Vector3f	Boid::Separate( BoidList const & boids ) const
{
//	Boid const *	pClosest	= FindClosest( boids );
//	Vector3f const	separation	= m_Position - pClosest->m_Position;
//	float const		distance	= separation.Length();
//
//	if ( distance < DESIRED_SEPARATION )
//	{
//		return separation * ( MAX_ACCELERATION / distance );	// Optimization: was separation.Normalize() * MAX_ACCELERATION
//	}
//	else
//	{
//		return Vector3f::ORIGIN;
//	}
	return Vector3f::ORIGIN;
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

Vector3f	Boid::Align( BoidList const & boids ) const
{
	Boid const * const	pClosest	= FindClosest( boids );

	if ( pClosest )
	{
		Vector3f	v	= pClosest->m_Velocity;
		return v.Normalize() * DESIRED_SPEED - m_Velocity;
	}
	else
	{
		return Vector3f::ORIGIN;
	}

}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

Vector3f	Boid::Congregate( BoidList const & boids ) const
{
	Boid const * const	pClosest	= FindClosest( boids );

	// If no boids are nearby, then no effect

	if ( !pClosest )
	{
		return Vector3f::ORIGIN;
	}

	Vector3f const	separation	= pClosest->m_Position - m_Position;
	float const		distance	= separation.Length();

	if ( Math::IsCloseToZero( distance ) )
	{
		return Vector3f::X_AXIS * MAX_ACCELERATION;
	}
	else if ( distance > DESIRED_SEPARATION )
	{
		return separation * ( MAX_ACCELERATION / distance );	// Optimization: was separation.Normalize() * MAX_ACCELERATION
	}
	else
	{
		return separation * ( -MAX_ACCELERATION / distance );	// Optimization: was separation.Normalize() * -MAX_ACCELERATION
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

Boid const *	Boid::FindClosest( BoidList const & boids ) const
{
	Boid const *	pClosest		= 0;
	float			closestDistance	= std::numeric_limits< float >::max();

	for ( BoidList::const_iterator ppB = boids.begin(); ppB != boids.end(); ++ppB )
	{
		float const	distance	= ( m_Position - (*ppB)->m_Position ).Length();

		if ( distance < closestDistance && distance < MAX_PERCEPTION_DISTANCE )
		{
			closestDistance = distance;
			pClosest = *ppB;
		}
	}

	return pClosest;
}