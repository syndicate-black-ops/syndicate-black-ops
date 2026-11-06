//
// vgui_SyndBrief.cpp
//
// SBO: CSyndBrief, the equipment ("briefing") menu: blades, light/assault weapons, cyberware,
// equipment, armor and agent sex, each with a picture and a description (vgui\\<item>.txt).
// The choices are kept in the viewport and sent to the server by the "Go !" button.
//

#include "VGUI_Font.h"
#include "VGUI_TextPanel.h"
#include "VGUI_LineBorder.h"
#include "VGUI_ScrollPanel.h"

#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "const.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"

//-----------------------------------------------------------------------------
CSyndBrief::CSyndBrief( int iTrans, int iRemoveMe, int x, int y, int wide, int tall ) : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	CSchemeManager *pSchemes;
	SchemeHandle_t hTitleScheme;
	SchemeHandle_t hTextScheme;
	Font *pTextFont;
	Font *pTitleFont;
	int r, g, b, a;
	char sz[64];
	int i;

	pSchemes = gViewPort->GetSchemeManager();
	hTitleScheme = pSchemes->getSchemeHandle("Title Font");
	hTextScheme = pSchemes->getSchemeHandle("Briefing Text");
	pTextFont = pSchemes->getFont(hTextScheme);
	pTitleFont = pSchemes->getFont(hTitleScheme);
	if ( gViewPort->m_iSBOInit != 1 )
	{
		gViewPort->m_iBlade = 1;
		gViewPort->m_iLight = 1;
		gViewPort->m_iHeavy = 1;
		gViewPort->m_iItem = 1;
		gViewPort->m_iCyberware = 1;
		gViewPort->m_iArmor = 1;
		gViewPort->m_iMaleFemale = 1;
		gViewPort->m_iSBOInit = 1;
	}

	m_pBackgroundPanel = new CTransparentPanel(128, XRES(0), YRES(0), XRES(640), YRES(480));
	m_pBackgroundPanel->setParent(this);

	m_pBladePanel = new CTransparentPanel(255, XRES(5), YRES(-5), XRES(180), YRES(99));
	m_pBladePanel->setParent(m_pBackgroundPanel);
	m_pBladeLabel = new Label("", XRES(0), YRES(5));
	m_pBladeLabel->setFont(pTitleFont);
	m_pBladeLabel->setParent(m_pBladePanel);
	pSchemes->getFgColor(hTitleScheme, r, g, b, a);
	m_pBladeLabel->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hTitleScheme, r, g, b, a);
	m_pBladeLabel->setBgColor(r, g, b, a);
	m_pBladeLabel->setContentAlignment( vgui::Label::a_west );
	sprintf(sz, "Blades");
	m_pBladeLabel->setText(sz);
	for ( i = 1; i <= 3; i++ )
	{
		m_pBladeButton[i] = new CommandButton("", 0, 0, XRES(180), YRES(20), true);
		m_pBladeButton[i]->setParent(m_pBladePanel);
		m_pBladeButton[i]->setContentAlignment( vgui::Label::a_west );
		m_pBladeButton[i]->addInputSignal(new CHandler_MenuButtonOver(this, i));
		if ( gViewPort->m_iBlade == i )
			m_pBladeButton[i]->setArmed( true );
	}
	m_pBladeButton[1]->setText("Fusion warhammer");
	m_pBladeButton[1]->setPos(XRES(0), YRES(33));
	m_pBladeButton[1]->addActionSignal(new CMenuHandler_Blade(1, m_pBladeButton[1], m_pBladeButton[2], m_pBladeButton[3]));
	m_pBladeButton[2]->setText("Chainsaw sword");
	m_pBladeButton[2]->setPos(XRES(0), YRES(56));
	m_pBladeButton[2]->addActionSignal(new CMenuHandler_Blade(2, m_pBladeButton[1], m_pBladeButton[2], m_pBladeButton[3]));
	m_pBladeButton[3]->setText("Energy sword");
	m_pBladeButton[3]->setPos(XRES(0), YRES(79));
	m_pBladeButton[3]->addActionSignal(new CMenuHandler_Blade(3, m_pBladeButton[1], m_pBladeButton[2], m_pBladeButton[3]));

	m_pLightPanel = new CTransparentPanel(255, XRES(5), YRES(105), XRES(180), YRES(122));
	m_pLightPanel->setParent(m_pBackgroundPanel);
	m_pLightLabel = new Label("", XRES(0), YRES(5));
	m_pLightLabel->setFont(pTitleFont);
	m_pLightLabel->setParent(m_pLightPanel);
	pSchemes->getFgColor(hTitleScheme, r, g, b, a);
	m_pLightLabel->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hTitleScheme, r, g, b, a);
	m_pLightLabel->setBgColor(r, g, b, a);
	m_pLightLabel->setContentAlignment( vgui::Label::a_west );
	sprintf(sz, "Light weapons");
	m_pLightLabel->setText(sz);
	for ( i = 1; i <= 4; i++ )
	{
		m_pLightButton[i] = new CommandButton("", 0, 0, XRES(180), YRES(20), true);
		m_pLightButton[i]->setParent(m_pLightPanel);
		m_pLightButton[i]->setContentAlignment( vgui::Label::a_west );
		m_pLightButton[i]->addInputSignal(new CHandler_MenuButtonOver(this, (i + 3)));
		if ( gViewPort->m_iLight == i )
			m_pLightButton[i]->setArmed( true );
	}
	m_pLightButton[1]->setText("Croon RC 28");
	m_pLightButton[1]->setPos(XRES(0), YRES(33));
	m_pLightButton[1]->addActionSignal(new CMenuHandler_Light(1, m_pLightButton[1], m_pLightButton[2], m_pLightButton[3], m_pLightButton[4]));
	m_pLightButton[2]->setText(".444k Bear killer");
	m_pLightButton[2]->setPos(XRES(0), YRES(56));
	m_pLightButton[2]->addActionSignal(new CMenuHandler_Light(2, m_pLightButton[1], m_pLightButton[2], m_pLightButton[3], m_pLightButton[4]));
	m_pLightButton[3]->setText("Canon of terror");
	m_pLightButton[3]->setPos(XRES(0), YRES(79));
	m_pLightButton[3]->addActionSignal(new CMenuHandler_Light(3, m_pLightButton[1], m_pLightButton[2], m_pLightButton[3], m_pLightButton[4]));
	m_pLightButton[4]->setText("Croon PM 66");
	m_pLightButton[4]->setPos(XRES(0), YRES(102));
	m_pLightButton[4]->addActionSignal(new CMenuHandler_Light(4, m_pLightButton[1], m_pLightButton[2], m_pLightButton[3], m_pLightButton[4]));

	m_pHeavyPanel = new CTransparentPanel(255, XRES(5), YRES(238), XRES(180), YRES(237));
	m_pHeavyPanel->setParent(m_pBackgroundPanel);
	m_pHeavyLabel = new Label("", XRES(0), YRES(5));
	m_pHeavyLabel->setFont(pTitleFont);
	m_pHeavyLabel->setParent(m_pHeavyPanel);
	pSchemes->getFgColor(hTitleScheme, r, g, b, a);
	m_pHeavyLabel->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hTitleScheme, r, g, b, a);
	m_pHeavyLabel->setBgColor(r, g, b, a);
	m_pHeavyLabel->setContentAlignment( vgui::Label::a_west );
	sprintf(sz, "Assault weapons");
	m_pHeavyLabel->setText(sz);
	for ( i = 1; i <= 9; i++ )
	{
		m_pHeavyButton[i] = new CommandButton("", 0, 0, XRES(180), YRES(20), true);
		m_pHeavyButton[i]->setParent(m_pHeavyPanel);
		m_pHeavyButton[i]->setContentAlignment( vgui::Label::a_west );
		m_pHeavyButton[i]->addInputSignal(new CHandler_MenuButtonOver(this, (i + 7)));
		if ( gViewPort->m_iHeavy == i )
			m_pHeavyButton[i]->setArmed( true );
	}
	m_pHeavyButton[1]->setText("Croon FA 18");
	m_pHeavyButton[1]->setPos(XRES(0), YRES(33));
	m_pHeavyButton[1]->addActionSignal(new CMenuHandler_Heavy(1, m_pHeavyButton[1], m_pHeavyButton[2], m_pHeavyButton[3], m_pHeavyButton[4], m_pHeavyButton[5], m_pHeavyButton[6], m_pHeavyButton[7], m_pHeavyButton[8], m_pHeavyButton[9]));
	m_pHeavyButton[2]->setText("Vindicio Federal");
	m_pHeavyButton[2]->setPos(XRES(0), YRES(56));
	m_pHeavyButton[2]->addActionSignal(new CMenuHandler_Heavy(2, m_pHeavyButton[1], m_pHeavyButton[2], m_pHeavyButton[3], m_pHeavyButton[4], m_pHeavyButton[5], m_pHeavyButton[6], m_pHeavyButton[7], m_pHeavyButton[8], m_pHeavyButton[9]));
	m_pHeavyButton[3]->setText("Vindico plasma sniper");
	m_pHeavyButton[3]->setPos(XRES(0), YRES(79));
	m_pHeavyButton[3]->addActionSignal(new CMenuHandler_Heavy(3, m_pHeavyButton[1], m_pHeavyButton[2], m_pHeavyButton[3], m_pHeavyButton[4], m_pHeavyButton[5], m_pHeavyButton[6], m_pHeavyButton[7], m_pHeavyButton[8], m_pHeavyButton[9]));
	m_pHeavyButton[4]->setText("Croon NM 33 sniper");
	m_pHeavyButton[4]->setPos(XRES(0), YRES(102));
	m_pHeavyButton[4]->addActionSignal(new CMenuHandler_Heavy(4, m_pHeavyButton[1], m_pHeavyButton[2], m_pHeavyButton[3], m_pHeavyButton[4], m_pHeavyButton[5], m_pHeavyButton[6], m_pHeavyButton[7], m_pHeavyButton[8], m_pHeavyButton[9]));
	m_pHeavyButton[5]->setText("Saan auto-shotgun");
	m_pHeavyButton[5]->setPos(XRES(0), YRES(125));
	m_pHeavyButton[5]->addActionSignal(new CMenuHandler_Heavy(5, m_pHeavyButton[1], m_pHeavyButton[2], m_pHeavyButton[3], m_pHeavyButton[4], m_pHeavyButton[5], m_pHeavyButton[6], m_pHeavyButton[7], m_pHeavyButton[8], m_pHeavyButton[9]));
	m_pHeavyButton[6]->setText("Benelli");
	m_pHeavyButton[6]->setPos(XRES(0), YRES(148));
	m_pHeavyButton[6]->addActionSignal(new CMenuHandler_Heavy(6, m_pHeavyButton[1], m_pHeavyButton[2], m_pHeavyButton[3], m_pHeavyButton[4], m_pHeavyButton[5], m_pHeavyButton[6], m_pHeavyButton[7], m_pHeavyButton[8], m_pHeavyButton[9]));
	m_pHeavyButton[7]->setText("Vindico grenade launcher");
	m_pHeavyButton[7]->setPos(XRES(0), YRES(171));
	m_pHeavyButton[7]->addActionSignal(new CMenuHandler_Heavy(7, m_pHeavyButton[1], m_pHeavyButton[2], m_pHeavyButton[3], m_pHeavyButton[4], m_pHeavyButton[5], m_pHeavyButton[6], m_pHeavyButton[7], m_pHeavyButton[8], m_pHeavyButton[9]));
	m_pHeavyButton[8]->setText("Vindicio Avenger minigun");
	m_pHeavyButton[8]->setPos(XRES(0), YRES(194));
	m_pHeavyButton[8]->addActionSignal(new CMenuHandler_Heavy(8, m_pHeavyButton[1], m_pHeavyButton[2], m_pHeavyButton[3], m_pHeavyButton[4], m_pHeavyButton[5], m_pHeavyButton[6], m_pHeavyButton[7], m_pHeavyButton[8], m_pHeavyButton[9]));
	m_pHeavyButton[9]->setText("Nuclear rocket launcher");
	m_pHeavyButton[9]->setPos(XRES(0), YRES(217));
	m_pHeavyButton[9]->addActionSignal(new CMenuHandler_Heavy(9, m_pHeavyButton[1], m_pHeavyButton[2], m_pHeavyButton[3], m_pHeavyButton[4], m_pHeavyButton[5], m_pHeavyButton[6], m_pHeavyButton[7], m_pHeavyButton[8], m_pHeavyButton[9]));

	m_pCyberwarePanel = new CTransparentPanel(255, XRES(485), YRES(232), XRES(150), YRES(168));
	m_pCyberwarePanel->setParent(m_pBackgroundPanel);
	m_pCyberwareLabel = new Label("", XRES(0), YRES(5));
	m_pCyberwareLabel->setFont(pTitleFont);
	m_pCyberwareLabel->setParent(m_pCyberwarePanel);
	pSchemes->getFgColor(hTitleScheme, r, g, b, a);
	m_pCyberwareLabel->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hTitleScheme, r, g, b, a);
	m_pCyberwareLabel->setBgColor(r, g, b, a);
	m_pCyberwareLabel->setContentAlignment( vgui::Label::a_west );
	sprintf(sz, "Cyber tech.");
	m_pCyberwareLabel->setText(sz);
	for ( i = 1; i <= 6; i++ )
	{
		m_pCyberwareButton[i] = new CommandButton("", 0, 0, XRES(150), YRES(20), true);
		m_pCyberwareButton[i]->setParent(m_pCyberwarePanel);
		m_pCyberwareButton[i]->setContentAlignment( vgui::Label::a_west );
		m_pCyberwareButton[i]->addInputSignal(new CHandler_MenuButtonOver(this, (i + 16)));
		if ( gViewPort->m_iCyberware == i )
			m_pCyberwareButton[i]->setArmed( true );
	}
	m_pCyberwareButton[1]->setText("Cyber legs");
	m_pCyberwareButton[1]->setPos(XRES(0), YRES(33));
	m_pCyberwareButton[1]->addActionSignal(new CMenuHandler_Cyberware(1, m_pCyberwareButton[1], m_pCyberwareButton[2], m_pCyberwareButton[3], m_pCyberwareButton[4], m_pCyberwareButton[5], m_pCyberwareButton[6]));
	m_pCyberwareButton[2]->setText("Targetting system");
	m_pCyberwareButton[2]->setPos(XRES(0), YRES(56));
	m_pCyberwareButton[2]->addActionSignal(new CMenuHandler_Cyberware(2, m_pCyberwareButton[1], m_pCyberwareButton[2], m_pCyberwareButton[3], m_pCyberwareButton[4], m_pCyberwareButton[5], m_pCyberwareButton[6]));
	m_pCyberwareButton[3]->setText("Medikit");
	m_pCyberwareButton[3]->setPos(XRES(0), YRES(79));
	m_pCyberwareButton[3]->addActionSignal(new CMenuHandler_Cyberware(3, m_pCyberwareButton[1], m_pCyberwareButton[2], m_pCyberwareButton[3], m_pCyberwareButton[4], m_pCyberwareButton[5], m_pCyberwareButton[6]));
	m_pCyberwareButton[4]->setText("Adrenaline surge");
	m_pCyberwareButton[4]->setPos(XRES(0), YRES(102));
	m_pCyberwareButton[4]->addActionSignal(new CMenuHandler_Cyberware(4, m_pCyberwareButton[1], m_pCyberwareButton[2], m_pCyberwareButton[3], m_pCyberwareButton[4], m_pCyberwareButton[5], m_pCyberwareButton[6]));
	m_pCyberwareButton[5]->setText("Assassin");
	m_pCyberwareButton[5]->setPos(XRES(0), YRES(125));
	m_pCyberwareButton[5]->addActionSignal(new CMenuHandler_Cyberware(5, m_pCyberwareButton[1], m_pCyberwareButton[2], m_pCyberwareButton[3], m_pCyberwareButton[4], m_pCyberwareButton[5], m_pCyberwareButton[6]));
	m_pCyberwareButton[6]->setText("Cyber eyes");
	m_pCyberwareButton[6]->setPos(XRES(0), YRES(148));
	m_pCyberwareButton[6]->addActionSignal(new CMenuHandler_Cyberware(6, m_pCyberwareButton[1], m_pCyberwareButton[2], m_pCyberwareButton[3], m_pCyberwareButton[4], m_pCyberwareButton[5], m_pCyberwareButton[6]));

	m_pItemPanel = new CTransparentPanel(255, XRES(485), YRES(-5), XRES(150), YRES(122));
	m_pItemPanel->setParent(m_pBackgroundPanel);
	m_pItemLabel = new Label("", XRES(0), YRES(5));
	m_pItemLabel->setFont(pTitleFont);
	m_pItemLabel->setParent(m_pItemPanel);
	pSchemes->getFgColor(hTitleScheme, r, g, b, a);
	m_pItemLabel->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hTitleScheme, r, g, b, a);
	m_pItemLabel->setBgColor(r, g, b, a);
	m_pItemLabel->setContentAlignment( vgui::Label::a_west );
	sprintf(sz, "Equipements");
	m_pItemLabel->setText(sz);
	for ( i = 1; i <= 4; i++ )
	{
		m_pItemButton[i] = new CommandButton("", 0, 0, XRES(150), YRES(20), true);
		m_pItemButton[i]->setParent(m_pItemPanel);
		m_pItemButton[i]->setContentAlignment( vgui::Label::a_west );
		m_pItemButton[i]->addInputSignal(new CHandler_MenuButtonOver(this, (i + 22)));
		if ( gViewPort->m_iItem == i )
			m_pItemButton[i]->setArmed( true );
	}
	m_pItemButton[1]->setText("Grenades");
	m_pItemButton[1]->setPos(XRES(0), YRES(33));
	m_pItemButton[1]->addActionSignal(new CMenuHandler_Item(1, m_pItemButton[1], m_pItemButton[2], m_pItemButton[3], m_pItemButton[4]));
	m_pItemButton[2]->setText("Mines");
	m_pItemButton[2]->setPos(XRES(0), YRES(56));
	m_pItemButton[2]->addActionSignal(new CMenuHandler_Item(2, m_pItemButton[1], m_pItemButton[2], m_pItemButton[3], m_pItemButton[4]));
	m_pItemButton[3]->setText("Satchels");
	m_pItemButton[3]->setPos(XRES(0), YRES(79));
	m_pItemButton[3]->addActionSignal(new CMenuHandler_Item(3, m_pItemButton[1], m_pItemButton[2], m_pItemButton[3], m_pItemButton[4]));
	m_pItemButton[4]->setText("Pitbull");
	m_pItemButton[4]->setPos(XRES(0), YRES(102));
	m_pItemButton[4]->addActionSignal(new CMenuHandler_Item(4, m_pItemButton[1], m_pItemButton[2], m_pItemButton[3], m_pItemButton[4]));

	m_pArmorPanel = new CTransparentPanel(255, XRES(485), YRES(114), XRES(150), YRES(122));
	m_pArmorPanel->setParent(m_pBackgroundPanel);
	m_pArmorLabel = new Label("", XRES(0), YRES(5));
	m_pArmorLabel->setFont(pTitleFont);
	m_pArmorLabel->setParent(m_pArmorPanel);
	pSchemes->getFgColor(hTitleScheme, r, g, b, a);
	m_pArmorLabel->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hTitleScheme, r, g, b, a);
	m_pArmorLabel->setBgColor(r, g, b, a);
	m_pArmorLabel->setContentAlignment( vgui::Label::a_west );
	sprintf(sz, "Armor");
	m_pArmorLabel->setText(sz);
	for ( i = 1; i <= 4; i++ )
	{
		m_pArmorButton[i] = new CommandButton("", 0, 0, XRES(150), YRES(20), true);
		m_pArmorButton[i]->setParent(m_pArmorPanel);
		m_pArmorButton[i]->setContentAlignment( vgui::Label::a_west );
		m_pArmorButton[i]->addInputSignal(new CHandler_MenuButtonOver(this, (i + 26)));
		if ( gViewPort->m_iArmor == i )
			m_pArmorButton[i]->setArmed( true );
	}
	m_pArmorButton[1]->setText("Armor skin");
	m_pArmorButton[1]->setPos(XRES(0), YRES(33));
	m_pArmorButton[1]->addActionSignal(new CMenuHandler_Armor(1, m_pArmorButton[1], m_pArmorButton[2], m_pArmorButton[3], m_pArmorButton[4]));
	m_pArmorButton[2]->setText("Cyber torso armor");
	m_pArmorButton[2]->setPos(XRES(0), YRES(56));
	m_pArmorButton[2]->addActionSignal(new CMenuHandler_Armor(2, m_pArmorButton[1], m_pArmorButton[2], m_pArmorButton[3], m_pArmorButton[4]));
	m_pArmorButton[3]->setText("Torso + skull armor");
	m_pArmorButton[3]->setPos(XRES(0), YRES(79));
	m_pArmorButton[3]->addActionSignal(new CMenuHandler_Armor(3, m_pArmorButton[1], m_pArmorButton[2], m_pArmorButton[3], m_pArmorButton[4]));
	m_pArmorButton[4]->setText("No armor");
	m_pArmorButton[4]->setPos(XRES(0), YRES(102));
	m_pArmorButton[4]->addActionSignal(new CMenuHandler_Armor(4, m_pArmorButton[1], m_pArmorButton[2], m_pArmorButton[3], m_pArmorButton[4]));

	m_pMaleFemalePanel = new CTransparentPanel(255, XRES(485), YRES(399), XRES(150), YRES(76));
	m_pMaleFemalePanel->setParent(m_pBackgroundPanel);
	m_pMaleFemaleLabel = new Label("", XRES(0), YRES(5));
	m_pMaleFemaleLabel->setFont(pTitleFont);
	m_pMaleFemaleLabel->setParent(m_pMaleFemalePanel);
	pSchemes->getFgColor(hTitleScheme, r, g, b, a);
	m_pMaleFemaleLabel->setFgColor(r, g, b, a);
	pSchemes->getBgColor(hTitleScheme, r, g, b, a);
	m_pMaleFemaleLabel->setBgColor(r, g, b, a);
	m_pMaleFemaleLabel->setContentAlignment( vgui::Label::a_west );
	sprintf(sz, "Who are you ?");
	m_pMaleFemaleLabel->setText(sz);
	for ( i = 1; i <= 2; i++ )
	{
		m_pMaleFemaleButton[i] = new CommandButton("", 0, 0, XRES(150), YRES(20), true);
		m_pMaleFemaleButton[i]->setParent(m_pMaleFemalePanel);
		m_pMaleFemaleButton[i]->setContentAlignment( vgui::Label::a_west );
		m_pMaleFemaleButton[i]->addInputSignal(new CHandler_MenuButtonOver(this, (i + 30)));
		if ( gViewPort->m_iMaleFemale == i )
			m_pMaleFemaleButton[i]->setArmed( true );
	}
	m_pMaleFemaleButton[1]->setText("Female agent");
	m_pMaleFemaleButton[1]->setPos(XRES(0), YRES(33));
	m_pMaleFemaleButton[1]->addActionSignal(new CMenuHandler_MaleFemale(1, m_pMaleFemaleButton[1], m_pMaleFemaleButton[2]));
	m_pMaleFemaleButton[2]->setText("Male agent");
	m_pMaleFemaleButton[2]->setPos(XRES(0), YRES(56));
	m_pMaleFemaleButton[2]->addActionSignal(new CMenuHandler_MaleFemale(2, m_pMaleFemaleButton[1], m_pMaleFemaleButton[2]));

	m_pButtonPanel = new CTransparentPanel(255, XRES(190), YRES(450), XRES(290), YRES(30));
	m_pButtonPanel->setParent(m_pBackgroundPanel);
	m_pCancelButton = new CommandButton(CHudTextMessage::BufferedLocaliseTextString("Cancel"), XRES(0), YRES(5), XRES(143), YRES(20), false);
	m_pCancelButton->setParent(m_pButtonPanel);
	m_pCancelButton->setContentAlignment( vgui::Label::a_center );
	m_pCancelButton->addActionSignal(new CMenuHandler_TextWindow(0));
	m_pCancelButton->addActionSignal(new CMenuHandler_StringCommand("", 1));
	m_pCancelButton->addActionSignal(new CMenuHandler_EnleveHighLight(m_pCancelButton));

	m_pGoButton[1] = new CommandButton("", XRES(147), YRES(5), XRES(143), YRES(20), true);
	m_pGoButton[1]->setParent(m_pButtonPanel);
	m_pGoButton[1]->setContentAlignment( vgui::Label::a_center );
	m_pGoButton[1]->addInputSignal(new CHandler_ButtonHighlight(m_pGoButton[1]));
	m_pGoButton[1]->setText("Go ! ");
	m_pGoButton[1]->addActionSignal(new CMenuHandler_Go(1));

	m_pInfoPanel = new CTransparentPanel(255, XRES(190), YRES(330), XRES(290), YRES(122));
	m_pInfoPanel->setParent(m_pBackgroundPanel);
	m_pInfoPanel->setBorder(new LineBorder(Color(41, 107, 34, 0)));
	m_pScrollPanel = new CTFScrollPanel(XRES(2), YRES(2), (XRES(290) - XRES(4)), (YRES(122) - YRES(4)));
	m_pScrollPanel->setParent(m_pInfoPanel);
	m_pScrollPanel->setScrollBarVisible( false, false );
	m_pInfoText = new TextPanel("", XRES(2), YRES(2), (XRES(290) - XRES(22)), (YRES(122) + YRES(200)));
	m_pInfoText->setParent( m_pScrollPanel->getClient() );
	m_pInfoText->setFont( pTextFont );
	pSchemes->getBgColor(hTitleScheme, r, g, b, a);
	m_pInfoText->setBgColor(r, g, b, a);
	m_pScrollPanel->validate();

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\hammer.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszBladeInfo[0], "Informations indisponibles!");
	else
		m_pszBladeInfo[0] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\chainsaw.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszBladeInfo[1], "Informations indisponibles!");
	else
		m_pszBladeInfo[1] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\sword.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszBladeInfo[2], "Informations indisponibles!");
	else
		m_pszBladeInfo[2] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\rc28.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszLightInfo[0], "Informations indisponibles!");
	else
		m_pszLightInfo[0] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\k444.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszLightInfo[1], "Informations indisponibles!");
	else
		m_pszLightInfo[1] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\canon.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszLightInfo[2], "Informations indisponibles!");
	else
		m_pszLightInfo[2] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\pm66.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszLightInfo[3], "Informations indisponibles!");
	else
		m_pszLightInfo[3] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\fa18.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszHeavyInfo[0], "Informations indisponibles!");
	else
		m_pszHeavyInfo[0] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\federal.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszHeavyInfo[1], "Informations indisponibles!");
	else
		m_pszHeavyInfo[1] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\sw.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszHeavyInfo[2], "Informations indisponibles!");
	else
		m_pszHeavyInfo[2] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\nm33.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszHeavyInfo[3], "Informations indisponibles!");
	else
		m_pszHeavyInfo[3] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\spa.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszHeavyInfo[4], "Informations indisponibles!");
	else
		m_pszHeavyInfo[4] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\ben.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszHeavyInfo[5], "Informations indisponibles!");
	else
		m_pszHeavyInfo[5] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\glauncher.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszHeavyInfo[6], "Informations indisponibles!");
	else
		m_pszHeavyInfo[6] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\chaingun.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszHeavyInfo[7], "Informations indisponibles!");
	else
		m_pszHeavyInfo[7] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\lr.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszHeavyInfo[8], "Informations indisponibles!");
	else
		m_pszHeavyInfo[8] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\legs.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszCyberwareInfo[0], "Informations indisponibles!");
	else
		m_pszCyberwareInfo[0] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\targeting.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszCyberwareInfo[1], "Informations indisponibles!");
	else
		m_pszCyberwareInfo[1] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\medikit.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszCyberwareInfo[2], "Informations indisponibles!");
	else
		m_pszCyberwareInfo[2] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\adrenaline.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszCyberwareInfo[3], "Informations indisponibles!");
	else
		m_pszCyberwareInfo[3] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\assassin.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszCyberwareInfo[4], "Informations indisponibles!");
	else
		m_pszCyberwareInfo[4] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\eye.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszCyberwareInfo[5], "Informations indisponibles!");
	else
		m_pszCyberwareInfo[5] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\grenade.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszItemInfo[0], "Informations indisponibles!");
	else
		m_pszItemInfo[0] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\tripmine.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszItemInfo[1], "Informations indisponibles!");
	else
		m_pszItemInfo[1] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\satchel.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszItemInfo[2], "Informations indisponibles!");
	else
		m_pszItemInfo[2] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\pit.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszItemInfo[3], "Informations indisponibles!");
	else
		m_pszItemInfo[3] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\skin.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszArmorInfo[0], "Informations indisponibles!");
	else
		m_pszArmorInfo[0] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\torso.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszArmorInfo[1], "Informations indisponibles!");
	else
		m_pszArmorInfo[1] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\skull.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszArmorInfo[3], "Informations indisponibles!");
	else
		m_pszArmorInfo[2] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\noarmor.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszArmorInfo[3], "Informations indisponibles!");
	else
		m_pszArmorInfo[3] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\female.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszMaleFemaleInfo[1], "Informations indisponibles!");
	else
		m_pszMaleFemaleInfo[1] = m_pFile;

	m_pFile = NULL;
	m_pFile = (char*)gEngfuncs.COM_LoadFile( "vgui\\male.txt", 5, NULL );
	if ( !m_pFile )
		strcpy(m_pszMaleFemaleInfo[0], "Informations indisponibles!");
	else
		m_pszMaleFemaleInfo[0] = m_pFile;

	m_pPicturePanel = new CTransparentPanel(255, XRES(190), YRES(5), XRES(290), YRES(320));
	m_pPicturePanel->setParent(m_pBackgroundPanel);

	if ( ScreenWidth > 800 )
	{
		m_pImagePanel = new CTransparentPanel(255, 0, 0, 320, 320);
		m_pImagePanel->setParent(m_pPicturePanel);
		m_pImagePanel->setPos( m_pPicturePanel->getWide() / 2 - 160, m_pPicturePanel->getTall() / 2 - 160 );
	}
	else
	{
		m_pImagePanel = new CTransparentPanel(255, 0, 0, 200, 200);
		m_pImagePanel->setParent(m_pPicturePanel);
		m_pImagePanel->setPos( m_pPicturePanel->getWide() / 2 - 100, m_pPicturePanel->getTall() / 2 - 100 );
	}

	m_pBladeImage[0] = new CImageLabel("hammer", XRES(0), YRES(0));
	m_pBladeImage[0]->setParent(m_pImagePanel);
	m_pBladeImage[0]->setVisible( false );
	m_pBladeImage[1] = new CImageLabel("chainsaw", XRES(0), YRES(0));
	m_pBladeImage[1]->setParent(m_pImagePanel);
	m_pBladeImage[1]->setVisible( false );
	m_pBladeImage[2] = new CImageLabel("sword", XRES(0), YRES(0));
	m_pBladeImage[2]->setParent(m_pImagePanel);
	m_pBladeImage[2]->setVisible( false );

	m_pLightImage[0] = new CImageLabel("rc28", XRES(0), YRES(0));
	m_pLightImage[0]->setParent(m_pImagePanel);
	m_pLightImage[0]->setVisible( false );
	m_pLightImage[1] = new CImageLabel("k444", XRES(0), YRES(0));
	m_pLightImage[1]->setParent(m_pImagePanel);
	m_pLightImage[1]->setVisible( false );
	m_pLightImage[2] = new CImageLabel("canon", XRES(0), YRES(0));
	m_pLightImage[2]->setParent(m_pImagePanel);
	m_pLightImage[2]->setVisible( false );
	m_pLightImage[3] = new CImageLabel("pm66", XRES(0), YRES(0));
	m_pLightImage[3]->setParent(m_pImagePanel);
	m_pLightImage[3]->setVisible( false );

	m_pHeavyImage[0] = new CImageLabel("fa18", XRES(0), YRES(0));
	m_pHeavyImage[0]->setParent(m_pImagePanel);
	m_pHeavyImage[0]->setVisible( false );
	m_pHeavyImage[1] = new CImageLabel("federal", XRES(0), YRES(0));
	m_pHeavyImage[1]->setParent(m_pImagePanel);
	m_pHeavyImage[1]->setVisible( false );
	m_pHeavyImage[2] = new CImageLabel("sw", XRES(0), YRES(0));
	m_pHeavyImage[2]->setParent(m_pImagePanel);
	m_pHeavyImage[2]->setVisible( false );
	m_pHeavyImage[3] = new CImageLabel("nm", XRES(0), YRES(0));
	m_pHeavyImage[3]->setParent(m_pImagePanel);
	m_pHeavyImage[3]->setVisible( false );
	m_pHeavyImage[4] = new CImageLabel("spaauto", XRES(0), YRES(0));
	m_pHeavyImage[4]->setParent(m_pImagePanel);
	m_pHeavyImage[4]->setVisible( false );
	m_pHeavyImage[5] = new CImageLabel("ben", XRES(0), YRES(0));
	m_pHeavyImage[5]->setParent(m_pImagePanel);
	m_pHeavyImage[5]->setVisible( false );
	m_pHeavyImage[6] = new CImageLabel("glauncher", XRES(0), YRES(0));
	m_pHeavyImage[6]->setParent(m_pImagePanel);
	m_pHeavyImage[6]->setVisible( false );
	m_pHeavyImage[7] = new CImageLabel("chaingun", XRES(0), YRES(0));
	m_pHeavyImage[7]->setParent(m_pImagePanel);
	m_pHeavyImage[7]->setVisible( false );
	m_pHeavyImage[8] = new CImageLabel("lr", XRES(0), YRES(0));
	m_pHeavyImage[8]->setParent(m_pImagePanel);
	m_pHeavyImage[8]->setVisible( false );

	m_pCyberwareImage[0] = new CImageLabel("legs", XRES(0), YRES(0));
	m_pCyberwareImage[0]->setParent(m_pImagePanel);
	m_pCyberwareImage[0]->setVisible( false );
	m_pCyberwareImage[1] = new CImageLabel("targeting", XRES(0), YRES(0));
	m_pCyberwareImage[1]->setParent(m_pImagePanel);
	m_pCyberwareImage[1]->setVisible( false );
	m_pCyberwareImage[2] = new CImageLabel("medikit", XRES(0), YRES(0));
	m_pCyberwareImage[2]->setParent(m_pImagePanel);
	m_pCyberwareImage[2]->setVisible( false );
	m_pCyberwareImage[3] = new CImageLabel("adrenaline", XRES(0), YRES(0));
	m_pCyberwareImage[3]->setParent(m_pImagePanel);
	m_pCyberwareImage[3]->setVisible( false );
	m_pCyberwareImage[4] = new CImageLabel("assassin", XRES(0), YRES(0));
	m_pCyberwareImage[4]->setParent(m_pImagePanel);
	m_pCyberwareImage[4]->setVisible( false );
	m_pCyberwareImage[5] = new CImageLabel("eye", XRES(0), YRES(0));
	m_pCyberwareImage[5]->setParent(m_pImagePanel);
	m_pCyberwareImage[5]->setVisible( false );

	m_pItemImage[0] = new CImageLabel("grenade", XRES(0), YRES(0));
	m_pItemImage[0]->setParent(m_pImagePanel);
	m_pItemImage[0]->setVisible( false );
	m_pItemImage[1] = new CImageLabel("tripmine", XRES(0), YRES(0));
	m_pItemImage[1]->setParent(m_pImagePanel);
	m_pItemImage[1]->setVisible( false );
	m_pItemImage[2] = new CImageLabel("satchel", XRES(0), YRES(0));
	m_pItemImage[2]->setParent(m_pImagePanel);
	m_pItemImage[2]->setVisible( false );
	m_pItemImage[3] = new CImageLabel("pit", XRES(0), YRES(0));
	m_pItemImage[3]->setParent(m_pImagePanel);
	m_pItemImage[3]->setVisible( false );

	m_pArmorImage[0] = new CImageLabel("skin", XRES(0), YRES(0));
	m_pArmorImage[0]->setParent(m_pImagePanel);
	m_pArmorImage[0]->setVisible( false );
	m_pArmorImage[1] = new CImageLabel("torso", XRES(0), YRES(0));
	m_pArmorImage[1]->setParent(m_pImagePanel);
	m_pArmorImage[1]->setVisible( false );
	m_pArmorImage[2] = new CImageLabel("skull", XRES(0), YRES(0));
	m_pArmorImage[2]->setParent(m_pImagePanel);
	m_pArmorImage[2]->setVisible( false );
	m_pArmorImage[3] = new CImageLabel("noarmor", XRES(0), YRES(0));
	m_pArmorImage[3]->setParent(m_pImagePanel);
	m_pArmorImage[3]->setVisible( false );

	m_pMaleFemaleImage[0] = new CImageLabel("female", XRES(0), YRES(0));
	m_pMaleFemaleImage[0]->setParent(m_pImagePanel);
	m_pMaleFemaleImage[0]->setVisible( false );
	m_pMaleFemaleImage[1] = new CImageLabel("male", XRES(0), YRES(0));
	m_pMaleFemaleImage[1]->setParent(m_pImagePanel);
	m_pMaleFemaleImage[1]->setVisible( false );

	Initialize();
}

void CSyndBrief::Initialize( void )
{
}

void CSyndBrief::Reset( void )
{
	CMenuPanel::Reset();
}

CMenuHandler_Blade::CMenuHandler_Blade( int iBlade, CommandButton *pButton1, CommandButton *pButton2, CommandButton *pButton3 )
{
	m_iCloseVGUIMenu = false;
	m_iBlade = iBlade;
	m_pButton1 = pButton1;
	m_pButton2 = pButton2;
	m_pButton3 = pButton3;
}

void CSyndBrief::SetActiveInfo( int iInput )
{
	if ( iInput == 1 )
	{
		m_pInfoText->setText( m_pszBladeInfo[0] );
		m_pBladeImage[0]->setVisible( true );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 2 )
	{
		m_pInfoText->setText( m_pszBladeInfo[1] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( true );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 3 )
	{
		m_pInfoText->setText( m_pszBladeInfo[2] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( true );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 4 )
	{
		m_pInfoText->setText( m_pszLightInfo[0] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( true );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 5 )
	{
		m_pInfoText->setText( m_pszLightInfo[1] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( true );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 6 )
	{
		m_pInfoText->setText( m_pszLightInfo[2] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( true );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 7 )
	{
		m_pInfoText->setText( m_pszLightInfo[3] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( true );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 8 )
	{
		m_pInfoText->setText( m_pszHeavyInfo[0] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( true );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 9 )
	{
		m_pInfoText->setText( m_pszHeavyInfo[1] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( true );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 10 )
	{
		m_pInfoText->setText( m_pszHeavyInfo[2] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( true );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 11 )
	{
		m_pInfoText->setText( m_pszHeavyInfo[3] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( true );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 12 )
	{
		m_pInfoText->setText( m_pszHeavyInfo[4] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( true );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 13 )
	{
		m_pInfoText->setText( m_pszHeavyInfo[5] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( true );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 14 )
	{
		m_pInfoText->setText( m_pszHeavyInfo[6] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( true );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 15 )
	{
		m_pInfoText->setText( m_pszHeavyInfo[7] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( true );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 16 )
	{
		m_pInfoText->setText( m_pszHeavyInfo[8] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( true );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 17 )
	{
		m_pInfoText->setText( m_pszCyberwareInfo[0] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( true );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 18 )
	{
		m_pInfoText->setText( m_pszCyberwareInfo[1] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( true );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 19 )
	{
		m_pInfoText->setText( m_pszCyberwareInfo[2] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( true );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 20 )
	{
		m_pInfoText->setText( m_pszCyberwareInfo[3] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( true );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 21 )
	{
		m_pInfoText->setText( m_pszCyberwareInfo[4] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( true );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 22 )
	{
		m_pInfoText->setText( m_pszCyberwareInfo[5] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( true );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 23 )
	{
		m_pInfoText->setText( m_pszItemInfo[0] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( true );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 24 )
	{
		m_pInfoText->setText( m_pszItemInfo[1] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( true );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 25 )
	{
		m_pInfoText->setText( m_pszItemInfo[2] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( true );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 26 )
	{
		m_pInfoText->setText( m_pszItemInfo[3] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( true );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 27 )
	{
		m_pInfoText->setText( m_pszArmorInfo[0] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( true );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 28 )
	{
		m_pInfoText->setText( m_pszArmorInfo[1] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( true );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 29 )
	{
		m_pInfoText->setText( m_pszArmorInfo[2] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( true );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 30 )
	{
		m_pInfoText->setText( m_pszArmorInfo[3] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( true );
		m_pMaleFemaleImage[1]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
	}
	if ( iInput == 31 )
	{
		m_pInfoText->setText( m_pszMaleFemaleInfo[1] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( true );
		m_pMaleFemaleImage[1]->setVisible( false );
	}
	if ( iInput == 32 )
	{
		m_pInfoText->setText( m_pszMaleFemaleInfo[0] );
		m_pBladeImage[0]->setVisible( false );
		m_pBladeImage[1]->setVisible( false );
		m_pBladeImage[2]->setVisible( false );
		m_pLightImage[0]->setVisible( false );
		m_pLightImage[1]->setVisible( false );
		m_pLightImage[2]->setVisible( false );
		m_pLightImage[3]->setVisible( false );
		m_pHeavyImage[0]->setVisible( false );
		m_pHeavyImage[1]->setVisible( false );
		m_pHeavyImage[2]->setVisible( false );
		m_pHeavyImage[3]->setVisible( false );
		m_pHeavyImage[4]->setVisible( false );
		m_pHeavyImage[5]->setVisible( false );
		m_pHeavyImage[6]->setVisible( false );
		m_pHeavyImage[7]->setVisible( false );
		m_pHeavyImage[8]->setVisible( false );
		m_pCyberwareImage[0]->setVisible( false );
		m_pCyberwareImage[1]->setVisible( false );
		m_pCyberwareImage[2]->setVisible( false );
		m_pCyberwareImage[3]->setVisible( false );
		m_pCyberwareImage[4]->setVisible( false );
		m_pCyberwareImage[5]->setVisible( false );
		m_pItemImage[0]->setVisible( false );
		m_pItemImage[1]->setVisible( false );
		m_pItemImage[2]->setVisible( false );
		m_pItemImage[3]->setVisible( false );
		m_pArmorImage[0]->setVisible( false );
		m_pArmorImage[1]->setVisible( false );
		m_pArmorImage[2]->setVisible( false );
		m_pArmorImage[3]->setVisible( false );
		m_pMaleFemaleImage[0]->setVisible( false );
		m_pMaleFemaleImage[1]->setVisible( true );
	}
	m_pScrollPanel->setScrollValue( 0, 0 );
}
