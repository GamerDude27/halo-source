//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HUD_RADAR_PANEL_H
#define HUD_RADAR_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include "hud_numericdisplay.h"
#include "c_vguiscreen.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>

#define RADAR_MAX_CONTACTS 24

using namespace vgui;

enum // If we have more than 16 types of contacts, RADAR_CONTACT_TYPE_BITS
{
	RADAR_CONTACT_ENEMY, // 'regular' sized enemy (Humanoid)
};

//-----------------------------------------------------------------------------
// Purpose: Shows the radar panel
//-----------------------------------------------------------------------------
class CRadarContact
{
public:
	Vector	m_vecOrigin;
	int		m_iType;
	float	m_flTimeToRemove;
};

class CHudRadarPanel : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudRadarPanel, Panel );

public:
	CHudRadarPanel(const char *pElementName);
	~CHudRadarPanel();
	
	virtual void Paint();
	virtual void Init(void);
	void VidInit(void);
	virtual void Reset(void);
	virtual void SetVisible(bool state);

	void AddRadarContact( const Vector &vecOrigin, int iType, float flTimeToLive );
	int FindRadarContact( const Vector &vecOrigin );
	void MaintainRadarContacts();

	void ClearAllRadarContacts() { m_iNumRadarContacts = 0; }

public:
	int m_ghostAlpha;			// How intense the alpha channel is for CRT ghosts
	float m_flTimeStopGhosting;
	float m_flTimeStartGhosting;

private:
	bool WorldToRadar( const Vector location, const Vector origin, const QAngle angles, float &x, float &y, float &z_delta, float &scale );

	void DrawPositionOnRadar( Vector vecPos, C_BasePlayer *pLocalPlayer, int type, int flags, int r, int g, int b, int a );
	void DrawIconOnRadar( Vector vecPos, C_BasePlayer *pLocalPlayer, int type, int flags, int r, int g, int b, int a );

	void FillRect(int x, int y, int w, int h);
	void DrawRadarDot(int x, int y, float z_diff, int iBaseDotSize, int flags, int r, int g, int b, int a);

	CRadarContact m_radarContacts[RADAR_MAX_CONTACTS];
	int m_iNumRadarContacts;
	int m_iImageID;
	int m_textureID_IconEnemy;
};

#endif // HUD_RADAR_PANEL_H