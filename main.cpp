/*****************************************************************************

                                   main.cpp

						Copyright 2001, John J. Bolton
	----------------------------------------------------------------------

	$Header: //depot/Flock/main.cpp#2 $

	$NoKeywords: $

*****************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <cmath>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include <gl/gl.h>
#include <gl/glu.h>
#include <gl/glaux.h>

#include "Glx/Glx.h"
#include "Wglx/Wglx.h"
#include "Wx/Wx.h"

#include "Misc/Etc.h"
#include "Misc/Random.h"
#include "Misc/Max.h"
#include "Misc/Trace.h"
#include "Math/Vector3f.h"
#include "Math/Constants.h"
#include "TgaFile/TgaFile.h"
#include "TerrainCamera/TerrainCamera.h"
#include "HeightField/HeightField.h"
#include "Water/Water.h"

#include "Flock.h"

int const	WATER_TO_LAND_RATIO	= 4;
float const	XY_SCALE			= 1.f;
float const	Z_SCALE				= 32.f;
int const	FLOCK_SIZE			= 100;

static LRESULT CALLBACK WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
static void InitializeRendering();
static void Display();
static void Reshape( int w, int h );
static void Update( HWND hWnd );
static void ReportGlErrors( GLenum error );
static void UpdateWater( DWORD newTime );
static void DrawWater();
static void DrawTerrain();

static void UpdateFlock( DWORD newTime );
static void DrawFlock();
static void DrawBoid();

static char						s_AppName[]	 = "Flock";
static char						s_TitleBar[] = "Flock";

static Glx::Lighting *			s_pLighting;
static Glx::DirectionalLight *	s_pDirectionalLight;

static Glx::Mesh *				s_pTerrainMesh;
static Glx::Mesh *				s_pBoidMesh;

static Glx::MipMappedTexture *	s_pTerrainTexture;

static Glx::Material *			s_pTerrainMaterial;
static Glx::Material *			s_pWaterMaterial;
static Glx::Material *			s_pBoidMaterial;

static TerrainCamera *			s_pCamera;
static Water *					s_pWater;
static HeightField *			s_pTerrain;
static float					s_CameraSpeed				= 2.f;
static double					s_SeaLevel					= Z_SCALE * .25;

static RandomFloat				s_RandomFloat( timeGetTime() );
static Random					s_Random( timeGetTime() );

static Flock					s_Flock;

static inline int WSizeX()
{
	return ( s_pTerrain->GetSizeX() - 1 ) / WATER_TO_LAND_RATIO + 1;
}

static inline int WSizeY()
{
	return ( s_pTerrain->GetSizeY() - 1 ) / WATER_TO_LAND_RATIO + 1;
}

static inline int W2T( int a )
{
	return a * WATER_TO_LAND_RATIO;
}

static inline int T2W( int a )
{
	return a / WATER_TO_LAND_RATIO;
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow )
{
	if ( Wx::RegisterWindowClass(	CS_OWNDC,
									( WNDPROC )WindowProc,
									0,
									0,
									hInstance,
									NULL,
									LoadCursor( NULL, IDC_ARROW ),
									NULL,
									NULL,
									s_AppName ) == NULL )
	{
		MessageBox( NULL, "Wx::RegisterWindowClass() failed.", "Error", MB_OK );
		exit( 1 );
	}

	HWND hWnd = CreateWindowEx( 0,
								s_AppName,
								s_TitleBar,
								WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
								0, 0, 500, 500,
								NULL,
								NULL,
								hInstance,
								NULL );

	if ( hWnd == NULL )
	{
		MessageBox( NULL, "CreateWindowEx() failed.", "Error", MB_OK );
		exit( 1 );
	}

	s_pTerrain = new HeightField( TgaFile( "hf.tga" ), XY_SCALE, Z_SCALE );
	{
		std::ostringstream	buffer;

		buffer << "Heightfield size - x: " << s_pTerrain->GetSizeX() << ", y: " << s_pTerrain->GetSizeY() << std::endl << std::ends;
		OutputDebugString( buffer.str().c_str() );
	}

	s_pWater = new Water( WSizeX(), WSizeY(), WATER_TO_LAND_RATIO * XY_SCALE, 20.f, .99f );

	// Generate the flock

	for ( int i = 0; i < FLOCK_SIZE; i++ )
	{
		Vector3f const	position( s_RandomFloat.Next( -5.f, 5.f ) * XY_SCALE,
								  s_RandomFloat.Next( -5.f, 5.f ) * XY_SCALE,
								  s_RandomFloat.Next(  0.f, 1.f ) );

		Vector3f const	velocity( s_RandomFloat.Next( -1.f, 1.f ) * Boid::DESIRED_SPEED,
								  s_RandomFloat.Next( -1.f, 1.f ) * Boid::DESIRED_SPEED,
								  s_RandomFloat.Next( -.1f, .1f ) * Boid::DESIRED_SPEED );

		Boid * const	pBoid	= new Boid( position, velocity );
		if ( !pBoid ) throw std::bad_alloc();

		s_Flock.push_back( pBoid );
	}

	HDC const	hDC	= GetDC( hWnd );
	int			rv;

	WGlx::SetPixelFormat( hDC, 0, true );

	{
		WGlx::CurrentRenderingContext	rc( hDC );	// Current rendering context

		InitializeRendering();

		s_pCamera			= new TerrainCamera( 60.f, 1.f, 1000.f,
												 Vector3f( 0.f, -s_pTerrain->GetSizeY() * XY_SCALE * .5f, Z_SCALE ),
												 -90.f, 90.f, 90.f );

		s_pCamera->GetDirection();
		s_pCamera->GetRight();
		s_pCamera->GetUp();
		s_pLighting			= new Glx::Lighting( Glx::Rgba( .4f, .4f, .4f ) );
		s_pDirectionalLight	= new Glx::DirectionalLight( GL_LIGHT0, Vector3f( 1.f, -1.f, 1.f ), Glx::Rgba( .6f, .6f, .6f ) );

		s_pWaterMaterial	= new Glx::Material( 0,
												 GL_MODULATE,
												 Glx::Rgba( .35f, .5f, 7.f, .75f )/*,
												 Glx::Rgba( 1.f, 1.f, 1.f, 1.f ), 128.f,
												 Glx::Lighting::BLACK,
												 GL_SMOOTH,
												 GL_FRONT_AND_BACK*/ );

		int const	terrainTextureSize	= 256;

		s_pTerrainTexture	= new Glx::MipMappedTexture( terrainTextureSize, terrainTextureSize, GL_BGR_EXT, GL_UNSIGNED_BYTE );

		try
		{
//			TgaFile	file( "256.tga" );
			TgaFile	file( "drock011.tga" );
//			TgaFile	file( "drock032.tga" );
			bool	ok;
			
			assert( file.m_ImageType == TgaFile::IMAGE_TRUECOLOR );
			assert( file.m_Height == terrainTextureSize && file.m_Width == terrainTextureSize );
			
			unsigned char *	s_pTextureData	= new unsigned char[ terrainTextureSize * terrainTextureSize * 4 ];
			if ( !s_pTextureData ) throw std::bad_alloc();
			
			ok = file.Read( s_pTextureData );
			assert( ok );
			
			s_pTerrainTexture->BuildAllMipMaps( s_pTextureData );
			
			delete[] s_pTextureData;
		}
		catch ( ... )
		{
			MessageBox(NULL, "Unable to load terrain texture.", "Water", MB_OK );
			throw;
		}

		s_pTerrainMaterial	= new Glx::Material( s_pTerrainTexture/*,
												 GL_MODULATE,
												 Glx::Lighting::WHITE,
												 Glx::Lighting::BLACK, 0.f,
												 Glx::Lighting::BLACK,
												 GL_SMOOTH,
												 GL_FRONT_AND_BACK*/ );

		s_pTerrainMesh			= new Glx::Mesh( s_pTerrainMaterial );
		s_pTerrainMesh->Begin();
		DrawTerrain();
		s_pTerrainMesh->End();

		s_pBoidMaterial	= new Glx::Material( 0,
											 GL_MODULATE,
											 Glx::Lighting::RED,
											 Glx::Lighting::BLACK, 0.f,
											 Glx::Lighting::BLACK,
											 GL_FLAT/*,
											 GL_FRONT_AND_BACK*/ );

		s_pBoidMesh			= new Glx::Mesh( s_pBoidMaterial );
		s_pBoidMesh->Begin();
		DrawBoid();
		s_pBoidMesh->End();

		SetTimer( hWnd, 0, 1000, NULL );

		ShowWindow( hWnd, nCmdShow );

		rv = Wx::MessageLoop( hWnd, Update );

		delete s_pBoidMesh;
		delete s_pBoidMaterial;
		delete s_pTerrainMesh;
		delete s_pTerrainMaterial;
		delete s_pTerrainTexture;
		delete s_pWaterMaterial;
		delete s_pDirectionalLight;
		delete s_pLighting;
		delete s_pCamera;

		for ( Flock::iterator ppB = s_Flock.begin(); ppB != s_Flock.end(); ++ppB )
		{
			delete *ppB;
		}
	}

	ReleaseDC( hWnd, hDC );
	DestroyWindow( hWnd );

	return rv;
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void Update( HWND hWnd )
{
	DWORD const	newTime	= timeGetTime();

	UpdateWater( newTime );
	UpdateFlock( newTime );

	InvalidateRect( hWnd, NULL, FALSE );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static LRESULT CALLBACK WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{ 
	switch( uMsg )
	{
	case WM_PAINT:
	{
		static PAINTSTRUCT ps;

		Display();
		BeginPaint( hWnd, &ps );
		EndPaint( hWnd, &ps );
		return 0;
	}

	case WM_SIZE:
		Reshape( LOWORD( lParam ), HIWORD( lParam ) );
		PostMessage( hWnd, WM_PAINT, 0, 0 );
		return 0;

	case WM_CHAR:
	{
		switch ( wParam )
		{
		case VK_ESCAPE:	// Quit
			PostQuitMessage( 0 );
			break;

		case ' ':	// Forward
			s_pCamera->Move( s_pCamera->GetDirection() * s_CameraSpeed );
			break;

		case 's':	// Backwards
			s_pCamera->Move( -s_pCamera->GetDirection() * s_CameraSpeed );
			break;

		case 'd':	// Strafe right
			s_pCamera->Move( s_pCamera->GetRight() * s_CameraSpeed );
			break;

		case 'a':	// Strafe left
			s_pCamera->Move( -s_pCamera->GetRight() * s_CameraSpeed );
			break;

		case 'w':	// Strafe up
			s_pCamera->Move( s_pCamera->GetUp() * s_CameraSpeed );
			break;

		case 'x':	// Strafe down
			s_pCamera->Move( -s_pCamera->GetUp() * s_CameraSpeed );
			break;

		case '1':
			s_SeaLevel -= Z_SCALE * .01;
			trace( "Sea level = %f\n", s_SeaLevel );
			break;

		case '2':
			s_SeaLevel += Z_SCALE * .01;
			trace( "Sea level = %f\n", s_SeaLevel );
			break;
		}

		return 0;
	}

	case WM_KEYDOWN:
		switch ( wParam )
		{
		case VK_UP:
			s_pCamera->Pitch( 5.f );
			break;

		case VK_DOWN:
			s_pCamera->Pitch( -5.f );
			break;

		case VK_LEFT:
			s_pCamera->Yaw( 5.f );
			break;

		case VK_RIGHT:
			s_pCamera->Yaw( -5.f );
			break;

		case VK_PRIOR:
			s_pCamera->Roll( 5.f );
			break;

		case VK_INSERT:
			s_pCamera->Roll( -5.f );
			break;
		}
		return 0;

	case WM_TIMER:
		{
			int const		RADIUS	=	3;
			float const		H		= Z_SCALE *.125f;
			float const		L		= 8.f;
			int	const		x0		= s_Random.Next( RADIUS, s_pWater->GetSizeX() - RADIUS );
			int const		y0		= s_Random.Next( RADIUS, s_pWater->GetSizeY() - RADIUS );

			for ( int i = -(RADIUS-1); i < RADIUS; i++ )
			{
				for ( int j = -(RADIUS-1); j < RADIUS; j++ )
				{
					s_pWater->GetData( x0+j, y0+i )->m_Z = H * cos( Math::TWO_PI * sqrt( i*i + j*j ) / L );
				}
			}
		}
		return 0;

	case WM_CLOSE:
		PostQuitMessage( 0 );
		return 0;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam ); 
} 


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void InitializeRendering()
{
	glClearColor( .65f, .80f, .90f, 1.f );
	glEnable( GL_DEPTH_TEST );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glCullFace( GL_BACK );
	glEnable( GL_CULL_FACE );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void UpdateWater( DWORD newTime )
{
	static DWORD	oldTime	= timeGetTime();

	// Compute the new heights

	s_pWater->Update( ( newTime - oldTime ) * .001f );

	// Apply a damping factor due to land

	int const					sx		= s_pWater->GetSizeX();
	int const					sy		= s_pWater->GetSizeY();

	for ( int y = 1; y < sy - 1; y++ )
	{
		for ( int x = 1; x < sx - 1; x++ )
		{
			int const					wx		= W2T( x );
			int const					wy		= W2T( y );
			double const				depth	= ( s_SeaLevel - s_pTerrain->GetZ( wx, wy ) );

			s_pWater->GetData( x, y )->m_Z	*= limit( 0., depth / ( s_SeaLevel * .25 ), 1. );
		}
	}

	// Save the new time

	oldTime = newTime;
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void DrawWater()
{
	glPushMatrix();

	int const					sx		= s_pWater->GetSizeX();
	int const					sy		= s_pWater->GetSizeY();
	float const					d		= s_pWater->GetXYScale();
	HeightField::Vertex const *	pData	= s_pWater->GetData();
	float const					x0		= -( sx - 1 ) * .5f * d;
	float const					y0		= -( sy - 1 ) * .5f * d;

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	s_pWaterMaterial->Apply();

	glEnableClientState( GL_NORMAL_ARRAY );
	glNormalPointer( GL_FLOAT, sizeof( HeightField::Vertex ), &pData->m_Normal );

	// Raise the water to sealevel

	glTranslatef( 0.f, 0.f, s_SeaLevel );

	for ( int i = 0; i < sy-1; i += 1 )
	{
		glBegin( GL_TRIANGLE_STRIP );

		for ( int j = 0; j < sx; j += 1 )
		{
			float const	x	= x0 + j * d;
			float const	tx	= float( j ) * .125f;//float( j ) / float( sx - 1 );

			HeightField::Vertex const * const	pa	= &pData[ i * sx + j ];
			HeightField::Vertex const * const	pb	= pa + sx;

			float const	yb	= y0 + ( i + 1 ) * d;
			float const	tyb	= float( i + 1 ) * .125f;//float( i + 1 ) / float( sy - 1 );
			float const	zb	= pb->m_Z;

#if defined( USING_WATER_TEXTURE )
			glTexCoord2f( tx, tyb );
#endif // defined( USING_WATER_TEXTURE )

			glArrayElement( ( i + 1 ) * sx + j );
//			glNormal3fv( pb->m_Normal.m_V );
			glVertex3f( x, yb, zb );

			float const	ya	= y0 + i * d;
			float const	tya	= float( i ) * .125f;//float( i ) / float( sy - 1 );
			float const	za	= pa->m_Z;

#if defined( USING_WATER_TEXTURE )
			glTexCoord2f( tx, tya );
#endif // defined( USING_WATER_TEXTURE )

			glArrayElement( i * sx + j );
//			glNormal3fv( pa->m_Normal.m_V );
			glVertex3f( x, ya, za );
		}

		glEnd();
	}

	glDisableClientState( GL_NORMAL_ARRAY );
	glDisable( GL_BLEND );

	glPopMatrix();
}



/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void DrawTerrain()
{
	int const					sx		= s_pTerrain->GetSizeX();
	int const					sy		= s_pTerrain->GetSizeY();
	HeightField::Vertex const *	pData	= s_pTerrain->GetData();
	float const					x0		= -( sx - 1 ) * .5f * XY_SCALE;
	float const					y0		= -( sy - 1 ) * .5f * XY_SCALE;

	s_pTerrainMaterial->Apply();

	glEnableClientState( GL_NORMAL_ARRAY );
	glNormalPointer( GL_FLOAT, sizeof( HeightField::Vertex ), &s_pTerrain->GetData()->m_Normal );

	for ( int i = 0; i < sy-1; i += 1 )
	{
		glBegin( GL_TRIANGLE_STRIP );

		for ( int j = 0; j < sx; j += 1 )
		{
			float const	x	= x0 + j * XY_SCALE;
			float const	tx	= float( j ) * .125f;//float( j ) / float( sx - 1 );

			HeightField::Vertex const * const	pa	= &pData[ i * sx + j ];
			HeightField::Vertex const * const	pb	= pa + sx;

			float const	yb	= y0 + ( i + 1 ) * XY_SCALE;
			float const	tyb	= float( i + 1 ) * .125f;//float( i + 1 ) / float( sy - 1 );
			float const	zb	= pb->m_Z;

			glTexCoord2f( tx, tyb );
			glArrayElement( ( i + 1 ) * sx + j );
//			glNormal3fv( pb->m_Normal.m_V );
			glVertex3f( x, yb, zb );

			float const	ya	= y0 + i * XY_SCALE;
			float const	tya	= float( i ) * .125f;//float( i ) / float( sy - 1 );
			float const	za	= pa->m_Z;

			glTexCoord2f( tx, tya );
			glArrayElement( i * sx + j );
//			glNormal3fv( pa->m_Normal.m_V );
			glVertex3f( x, ya, za );
		}

		glEnd();
	}

	glDisableClientState( GL_NORMAL_ARRAY );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void UpdateFlock( DWORD newTime )
{
	static DWORD	oldTime	= timeGetTime();
	int const		dt		= int( newTime - oldTime );

	if ( dt <= 0 )
		return;

	s_Flock.Update( dt * .001f, *s_pTerrain, XY_SCALE, s_SeaLevel );

	oldTime = newTime;
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void DrawFlock()
{
	for ( Flock::iterator ppB = s_Flock.begin(); ppB != s_Flock.end(); ++ppB )
	{
		Boid const &	boid	= **ppB;

		glPushMatrix();

		glTranslatef( boid.m_Position.m_X, boid.m_Position.m_Y, boid.m_Position.m_Z );

		s_pBoidMesh->Apply();

		glPopMatrix();
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void DrawBoid()
{
	s_pBoidMaterial->Apply();

	glBegin( GL_TRIANGLE_FAN );

	float const	A	= 0.5 * Math::SQRT_OF_2_OVER_2;
	float const	B	= 1.f;

	glNormal3f( 0.f, 1.f, 0.f );
	glVertex3f( 0.f, 0.f, 0.f );

	glNormal3f( 0.f, 1.f, 0.f );
	glVertex3f( A, A, B );

	glNormal3f( -Math::SQRT_OF_2_OVER_2, Math::SQRT_OF_2_OVER_2, 0.f );
	glVertex3f( 0.f, 0.f, B );

	glNormal3f( Math::SQRT_OF_2_OVER_2, Math::SQRT_OF_2_OVER_2, 0.f );
	glVertex3f( -A, A, B );

	glEnd();
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void Display()
{
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glMatrixMode( GL_MODELVIEW );

	glLoadIdentity();

	// Place the camera

	s_pCamera->Look();

	// Lights

	s_pDirectionalLight->Apply();

	// Draw the flock

	DrawFlock();

	// Draw the terrain

	s_pTerrainMesh->Apply();

	// Draw the water

	DrawWater();

	// Display the scene

	glFlush();
	SwapBuffers( wglGetCurrentDC() );

	GLenum const	error	= glGetError();
	if ( error != GL_NO_ERROR )
	{
		ReportGlErrors( error );
	}
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void ReportGlErrors( GLenum error )
{
	std::ostringstream buffer;

	buffer << "*** glError returned errors: " << std::endl;
		
	do
	{
		buffer << gluErrorString( error ) << std::endl;

	} while ( ( error = glGetError() ) != GL_NO_ERROR );

	buffer << std::ends;

	OutputDebugString( buffer.str().c_str() );
}


/********************************************************************************************************************/
/*																													*/
/*																													*/
/********************************************************************************************************************/

static void Reshape( int w, int h )
{
	s_pCamera->Reshape( w, h );
}


