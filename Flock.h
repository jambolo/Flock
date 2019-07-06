#if !defined( FLOCK_H_INCLUDED )
#define FLOCK_H_INCLUDED

#pragma once

/*****************************************************************************

                                   Flock.h

						Copyright 2001, John J. Bolton
	----------------------------------------------------------------------

	$Header: //depot/Flock/Flock.h#2 $

	$NoKeywords: $

*****************************************************************************/

#include "Boid.h"

class HeightField;

/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

class Flock : public BoidList
{
public:

	Flock();
	virtual ~Flock();

	void Update( float dt, HeightField const & terrain, float xyScale, float seaLevel );
};

#endif // !defined( FLOCK_H_INCLUDED )
