//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_radar_panel.h"
#include "hud_macros.h"
#include "hud_numericdisplay.h"
#include "mapoverview.h"
#include "c_baseplayer.h"
#include "iclientmode.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/ILocalize.h"

#include <coordsize.h>

#include "tier0/memdbgon.h"

#define RADAR_DOT_NORMAL	  0
#define RADAR_IGNORE_Z		  (1<<6) //always draw this item as if it was at the same Z as the player
#define RADAR_MAX_GHOST_ALPHA 25

DECLARE_HUDELEMENT(CHudRadarPanel);

#define RADAR_PANEL_MATERIAL			"vgui/screens/radar"
#define RADAR_CONTACT_ENEMY_MATERIAL	"vgui/icons/icon_enemy" // Generic enemy

static CHudRadarPanel *s_Radar = NULL;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudRadarPanel::CHudRadarPanel(const char *pElementName) : CHudElement(pElementName), CHudNumericDisplay(NULL, "HudRadar")
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_iImageID = -1;
	m_textureID_IconEnemy = -1;

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//---------------------------------------------------------
// Purpose: 
//---------------------------------------------------------
CHudRadarPanel::~CHudRadarPanel()
{
	s_Radar = NULL;

#if defined(_X360)
	if( m_iImageID != -1 )
	{
		surface()->DestroyTextureID( m_iImageID );
		m_iImageID  = -1;
	}

	if( m_textureID_IconEnemy != -1 )
	{
		surface()->DestroyTextureID( m_textureID_IconEnemy );
		m_textureID_IconEnemy  = -1;
	}
#endif//_X360
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CHudRadarPanel::Init()
{
	Reset();
}

//---------------------------------------------------------
// Purpose: 
//---------------------------------------------------------
void CHudRadarPanel::VidInit(void)
{
}

//---------------------------------------------------------
// Purpose: 
//---------------------------------------------------------
void CHudRadarPanel::Reset(void)
{
	ClearAllRadarContacts();
	s_Radar = this;

	m_ghostAlpha = 0;
	m_flTimeStartGhosting = gpGlobals->curtime + 1.0f;
}

//---------------------------------------------------------
// Purpose: Register a radar contact in the list of contacts
//---------------------------------------------------------
void CHudRadarPanel::AddRadarContact( const Vector &vecOrigin, int iType, float flTimeToLive )
{
	if( m_iNumRadarContacts == RADAR_MAX_CONTACTS )
	{
		return;
	}

	Vector v = vecOrigin;
	int iExistingContact = FindRadarContact( vecOrigin );

	if( iExistingContact > -1 )
	{
		// Just update this contact.
		m_radarContacts[iExistingContact].m_flTimeToRemove = gpGlobals->curtime + flTimeToLive;
		return;
	}

	m_radarContacts[m_iNumRadarContacts].m_vecOrigin = vecOrigin;
	m_radarContacts[m_iNumRadarContacts].m_iType = iType;
	m_radarContacts[m_iNumRadarContacts].m_flTimeToRemove = gpGlobals->curtime + flTimeToLive;
	m_iNumRadarContacts++;
}

//---------------------------------------------------------
// Purpose: Search the contact list for a specific contact
//---------------------------------------------------------
int CHudRadarPanel::FindRadarContact( const Vector &vecOrigin )
{
	for( int i = 0 ; i < m_iNumRadarContacts ; i++ )
	{
		if( m_radarContacts[ i ].m_vecOrigin == vecOrigin )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Go through all radar targets and see if any have expired. 
//			If yes, remove them from the list.
//-----------------------------------------------------------------------------
void CHudRadarPanel::MaintainRadarContacts()
{
	bool bKeepWorking = true;
	while (bKeepWorking)
	{
		bKeepWorking = false;
		for (int i = 0; i < m_iNumRadarContacts; i++)
		{
			CRadarContact *pContact = &m_radarContacts[i];
			if (gpGlobals->curtime >= pContact->m_flTimeToRemove)
			{
				// Time for this guy to go. Easiest thing is just to copy the last element 
				// into this element's spot and then decrement the count of entities.
				bKeepWorking = true;

				m_radarContacts[i] = m_radarContacts[m_iNumRadarContacts - 1];
				m_iNumRadarContacts--;
				break;
			}
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------
void CHudRadarPanel::SetVisible(bool state)
{
	BaseClass::SetVisible(state);

	if( g_pMapOverview  &&  g_pMapOverview->GetMode() == CMapOverview::MAP_MODE_RADAR )
	{
		// We are the hud element still, but he is in charge of the new style now.
		g_pMapOverview->SetVisible( state );		
	}
}

#define RADAR_BLIP_FADE_TIME 1.0f
#define RADAR_USE_ICONS		 1
//-----------------------------------------------------------------------------
// Purpose: Draws the radar panel
//-----------------------------------------------------------------------------
void CHudRadarPanel::Paint()
{
	if (m_iImageID == -1)
	{
		// Set up the image ID's if they've somehow gone bad.
		m_textureID_IconEnemy = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile(m_textureID_IconEnemy, RADAR_CONTACT_ENEMY_MATERIAL, true, false);

		m_iImageID = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile(m_iImageID, RADAR_PANEL_MATERIAL, true, false);
	}

	// Draw the radar background.
	int wide, tall;
	GetSize(wide, tall);
	int alpha = 255;
	surface()->DrawSetColor(255, 255, 255, alpha);
	surface()->DrawSetTexture(m_iImageID);
	surface()->DrawTexturedRect(0, 0, wide, tall);

	// Manage the CRT 'ghosting' effect
	if (gpGlobals->curtime > m_flTimeStartGhosting)
	{
		if (m_ghostAlpha < RADAR_MAX_GHOST_ALPHA)
		{
			m_ghostAlpha++;
		}
		else
		{
			m_flTimeStartGhosting = FLT_MAX;
			m_flTimeStopGhosting = gpGlobals->curtime + RandomFloat(1.0f, 2.0f);// How long to ghost for
		}
	}
	else if (gpGlobals->curtime > m_flTimeStopGhosting)
	{
		// We're supposed to stop ghosting now.
		if (m_ghostAlpha > 0)
		{
			// Still fading the effects.
			m_ghostAlpha--;
		}
		else
		{
			// DONE fading the effects. Now stop ghosting for a short while
			m_flTimeStartGhosting = gpGlobals->curtime + RandomFloat(2.0f, 3.0f);// how long between ghosts
			m_flTimeStopGhosting = FLT_MAX;
		}
	}

	// Now go through the list of radar targets and represent them on the radar screen
	// by drawing their icons on top of the background.
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	for (int i = 0; i < m_iNumRadarContacts; i++)
	{
		int alpha = 90;
		CRadarContact *pContact = &m_radarContacts[i];
		float deltaT = pContact->m_flTimeToRemove - gpGlobals->curtime;
		if (deltaT < RADAR_BLIP_FADE_TIME)
		{
			float factor = deltaT / RADAR_BLIP_FADE_TIME;

			alpha = (int)(((float)alpha) * factor);

			if (alpha < 10)
			{
				alpha = 10;
			}
		}

		if (RADAR_USE_ICONS)
		{
			int flicker = RandomInt(0, 30);
			DrawIconOnRadar(pContact->m_vecOrigin, pLocalPlayer, pContact->m_iType, RADAR_IGNORE_Z, 255, 255, 255, alpha + flicker);
		}
		else
		{
			DrawPositionOnRadar(pContact->m_vecOrigin, pLocalPlayer, pContact->m_iType, RADAR_IGNORE_Z, 255, 255, 255, alpha);
		}
	}

	MaintainRadarContacts();
}

ConVar hud_radar_range("hud_radar_range", "3000"); // 180 feet
//---------------------------------------------------------
// Scale maps the distance of the target from the radar 
// source. 
//
//		1.0 = target at or beyond radar range.
//		0.5 = target at (radar_range * 0.5) units distance
//		0.25 = target at (radar_range * 0.25) units distance
//		-etc-
//---------------------------------------------------------
bool CHudRadarPanel::WorldToRadar( const Vector location, const Vector origin, const QAngle angles, float &x, float &y, float &z_delta, float &scale )
{
	bool bInRange = true;

	float x_diff = location.x - origin.x;
	float y_diff = location.y - origin.y;

	// Supply epsilon values to avoid divide-by-zero
	if(x_diff == 0)
	{
		x_diff = 0.00001f;
	}

	if(y_diff == 0)
	{
		y_diff = 0.00001f;
	}

	int iRadarRadius = GetWide(); //width of the panel
	float fRange = hud_radar_range.GetFloat();

	// This magic /2.15 makes the radar scale seem smaller than the VGUI panel so the icons clamp
	// to the outer ring in the radar graphic, not the very edge of the panel itself.
	float fScale = (iRadarRadius/2.15f) / fRange;					

	float flOffset = atan(y_diff/x_diff);
	flOffset *= 180;
	flOffset /= M_PI;

	if ((x_diff < 0) && (y_diff >= 0))
	{
		flOffset = 180 + flOffset;
	}
	else if ((x_diff < 0) && (y_diff < 0))
	{
		flOffset = 180 + flOffset;
	}
	else if ((x_diff >= 0) && (y_diff < 0))
	{
		flOffset = 360 + flOffset;
	}

	y_diff = -1*(sqrt((x_diff)*(x_diff) + (y_diff)*(y_diff)));
	x_diff = 0;

	flOffset = angles.y - flOffset;

	flOffset *= M_PI;
	flOffset /= 180;		// now theta is in radians

	// Transform relative to radar source
	float xnew_diff = x_diff * cos(flOffset) - y_diff * sin(flOffset);
	float ynew_diff = x_diff * sin(flOffset) + y_diff * cos(flOffset);

	if ( (-1 * y_diff) > fRange )
	{
		float flScale;

		flScale = ( -1 * y_diff) / fRange;

		xnew_diff /= (flScale);
		ynew_diff /= (flScale);

		bInRange = false;

		scale = 1.0f;
	}
	else
	{
		// scale
		float flDist = sqrt( ((xnew_diff)*(xnew_diff) + (ynew_diff)*(ynew_diff)) );
		scale = flDist / fRange;
	}

	// Scale the dot's position to match radar scale
	xnew_diff *= fScale;
	ynew_diff *= fScale;

	// Translate to screen coordinates
	x = (iRadarRadius/2) + (int)xnew_diff;
	y = (iRadarRadius/2) + (int)ynew_diff;
	z_delta = 0.0f;

	return bInRange;
}

//-----------------------------------------------------------------------------
// Purpose: Draws position on radar panel
//-----------------------------------------------------------------------------
void CHudRadarPanel::DrawPositionOnRadar( Vector vecPos, C_BasePlayer *pLocalPlayer, int type, int flags, int r, int g, int b, int a )
{
	float x, y, z_delta;
	int iBaseDotSize = 3;
	
	QAngle viewAngle = pLocalPlayer->EyeAngles();
	
	/*
	if( m_pVehicle != NULL )
	{
		viewAngle = m_pVehicle->GetAbsAngles();
		viewAngle.y += 90.0f;
	}
	*/

	float flScale;

	WorldToRadar( vecPos, pLocalPlayer->GetAbsOrigin(), viewAngle, x, y, z_delta, flScale );

	if( flags & RADAR_IGNORE_Z )
	{
		z_delta = 0;
	}

	switch( type )
	{
	case RADAR_CONTACT_ENEMY:
		r = 255;	g = 0;	b = 0;
		iBaseDotSize *= 2;
		break;
	}

	DrawRadarDot( x, y, z_delta, iBaseDotSize, flags, r, g, b, a );
}

//---------------------------------------------------------
// Purpose: Compute the proper position on the radar screen
//			for this object's position relative to the player.
//			Then draw the icon in the proper location on the
//			radar screen.
//---------------------------------------------------------
#define RADAR_ICON_MIN_SCALE 0.75f
#define RADAR_ICON_MAX_SCALE 1.0f
void CHudRadarPanel::DrawIconOnRadar(Vector vecPos, C_BasePlayer *pLocalPlayer, int type, int flags, int r, int g, int b, int a)
{
	float x, y, z_delta;
	int wide, tall;

	// for 'ghosting' CRT effects:
	int xmod;
	int ymod;
	int xoffset;
	int yoffset;

	// Assume we're going to use the player's location and orientation
	QAngle viewAngle = pLocalPlayer->EyeAngles();
	Vector viewOrigin = pLocalPlayer->GetAbsOrigin();

	/*
	// However, happily use those of the vehicle if available!
	if (m_pVehicle != NULL)
	{
		viewAngle = m_pVehicle->GetAbsAngles();
		viewAngle.y += 90.0f;
		viewOrigin = m_pVehicle->WorldSpaceCenter();
	}
	*/

	float flScale;

	WorldToRadar(vecPos, viewOrigin, viewAngle, x, y, z_delta, flScale);

	flScale = RemapVal(flScale, 1.0f, 0.0f, RADAR_ICON_MIN_SCALE, RADAR_ICON_MAX_SCALE);

	// Get the correct icon for this type of contact
	int iTextureID_Icon = -1;

	switch (type)
	{
	case RADAR_CONTACT_ENEMY:
		iTextureID_Icon = m_textureID_IconEnemy;
		break;
	default:
		return;
		break;
	}

	surface()->DrawSetColor(r, g, b, a);
	surface()->DrawSetTexture(iTextureID_Icon);
	surface()->DrawGetTextureSize(iTextureID_Icon, wide, tall);

	wide = (int((float)wide * flScale));
	tall = (int((float)tall * flScale));

	// Center the icon around its position.
	x -= (wide >> 1);
	y -= (tall >> 1);

	surface()->DrawTexturedRect(x, y, x + wide, y + tall);

	// Draw the crt 'ghost' if the icon is not pegged to the outer rim
	if (flScale > RADAR_ICON_MIN_SCALE && m_ghostAlpha > 0)
	{
		surface()->DrawSetColor(r, g, b, m_ghostAlpha);
		xmod = RandomInt(1, 4);
		ymod = RandomInt(1, 4);
		xoffset = RandomInt(-1, 1);
		yoffset = RandomInt(-1, 1);
		x -= (xmod - xoffset);
		y -= (ymod - yoffset);
		wide += (xmod + xoffset);
		tall += (ymod + yoffset);
		surface()->DrawTexturedRect(x, y, x + wide, y + tall);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudRadarPanel::FillRect(int x, int y, int w, int h)
{
	int panel_x, panel_y, panel_w, panel_h;
	GetBounds(panel_x, panel_y, panel_w, panel_h);
	surface()->DrawFilledRect(x, y, x + w, y + h);
}

//-----------------------------------------------------------------------------
// Purpose: Draws the radar dot
//-----------------------------------------------------------------------------
void CHudRadarPanel::DrawRadarDot( int x, int y, float z_diff, int iBaseDotSize, int flags, int r, int g, int b, int a )
{
	surface()->DrawSetColor( r, g, b, a );

	if ( z_diff < -128 ) // below the player
	{
		z_diff *= -1;

		if ( z_diff > 3096 )
		{
			z_diff = 3096;
		}

		int iBar = (int)( z_diff / 400 ) + 2;

		// Draw an upside-down T shape to symbolize the dot is below the player.

		iBaseDotSize /= 2;

		// Horizontal
		FillRect( x-(2*iBaseDotSize), y, 5*iBaseDotSize, iBaseDotSize );

		// Vertical
		FillRect( x, y - iBar*iBaseDotSize, iBaseDotSize, iBar*iBaseDotSize );
	}
	else if ( z_diff > 128 ) // above the player
	{
		if ( z_diff > 3096 )
		{
			z_diff = 3096;
		}

		int iBar = (int)( z_diff / 400 ) + 2;

		iBaseDotSize /= 2;
		
		// Draw a T shape to symbolize the dot is above the player.

		// Horizontal
		FillRect( x-(2*iBaseDotSize), y, 5*iBaseDotSize, iBaseDotSize );

		// Vertical
		FillRect( x, y, iBaseDotSize, iBar*iBaseDotSize );
	}
	else 
	{
		FillRect( x, y, iBaseDotSize, iBaseDotSize );
	}
}