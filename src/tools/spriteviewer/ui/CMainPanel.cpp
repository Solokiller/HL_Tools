#include <memory>

#include <wx/notebook.h>
#include <wx/gbsizer.h>

#include "C3DView.h"

#include "engine/shared/sprite/sprite.h"
#include "engine/shared/sprite/CSprite.h"
#include "game/entity/CSpriteEntity.h"
#include "game/entity/CBaseEntityList.h"

#include "CSpriteViewer.h"

#include "CMainPanel.h"

namespace sprview
{
wxBEGIN_EVENT_TABLE( CMainPanel, wxPanel )
	EVT_NOTEBOOK_PAGE_CHANGED( wxID_MAIN_PAGECHANGED, CMainPanel::PageChanged )
wxEND_EVENT_TABLE()

CMainPanel::CMainPanel( wxWindow* pParent, CSpriteViewer* const pSpriteViewer )
	: wxPanel( pParent )
	, m_pSpriteViewer( pSpriteViewer )
{
	wxASSERT( pSpriteViewer != nullptr );

	m_p3DView = new C3DView( this, m_pSpriteViewer, this );

	m_pControlPanel = new wxPanel( this );

	m_pMainControlBar = new wxPanel( m_pControlPanel );

	m_pFPS = new wxStaticText( m_pMainControlBar, wxID_ANY, "FPS: 0" );

	m_pControlPanels = new wxNotebook( m_pControlPanel, wxID_MAIN_PAGECHANGED );

	//TODO:
	//m_pControlPanels->SetSelection( 0 );

	//Layout
	wxGridBagSizer* pBarSizer = new wxGridBagSizer( 5, 5 );

	pBarSizer->Add( m_pFPS, wxGBPosition( 0, 3 ), wxGBSpan( 1, 1 ), wxALIGN_CENTER );

	m_pMainControlBar->SetSizer( pBarSizer );

	wxBoxSizer* pPanelSizer = new wxBoxSizer( wxVERTICAL );

	pPanelSizer->Add( m_pMainControlBar, wxSizerFlags().Expand() );

	pPanelSizer->Add( m_pControlPanels, wxSizerFlags().Expand() );

	m_pControlPanel->SetSizer( pPanelSizer );

	wxBoxSizer* pSizer = new wxBoxSizer( wxVERTICAL );

	//3D view takes up 3/4th of the main area
	pSizer->Add( m_p3DView, wxSizerFlags().Expand().Proportion( 3 ) );
	pSizer->Add( m_pControlPanel, wxSizerFlags().Expand().Proportion( 1 ) );

	this->SetSizer( pSizer );

	InitializeUI();
}

CMainPanel::~CMainPanel()
{
}

void CMainPanel::RunFrame()
{
	++m_uiCurrentFPS;

	const long long iCurrentTick = GetCurrentTick();

	m_p3DView->UpdateView();

	//Update FPS.
	if( iCurrentTick - m_iLastFPSUpdate >= 1000 )
	{
		m_iLastFPSUpdate = iCurrentTick;

		m_pFPS->SetLabelText( wxString::Format( "FPS: %u", m_uiCurrentFPS ) );

		m_uiCurrentFPS = 0;
	}
}

void CMainPanel::Draw3D( const wxSize& size )
{
	const int iPage = m_pControlPanels->GetSelection();

	if( iPage != wxNOT_FOUND )
	{
		/*
		CBaseControlPanel* const pPage = static_cast<CBaseControlPanel*>( m_pControlPanels->GetPage( iPage ) );

		pPage->Draw3D( size );
		*/
	}
}

bool CMainPanel::LoadSprite( const wxString& szFilename )
{
	m_p3DView->PrepareForLoad();

	m_pSpriteViewer->GetState()->ResetModelData();

	m_pSpriteViewer->GetState()->ClearEntity();

	auto szCFilename = szFilename.char_str( wxMBConvUTF8() );

	sprite::msprite_t* pSprite;

	const auto result = sprite::LoadSprite( szFilename.c_str(), pSprite );

	if( !result )
	{
		wxMessageBox( wxString::Format( "Error loading sprite \"%s\"\n", szFilename ), "Error" );
		return false;
	}

	CSpriteEntity* pEntity = static_cast<CSpriteEntity*>( CBaseEntity::Create( "sprite", glm::vec3(), glm::vec3(), false ) );

	if( pEntity )
	{
		//TODO:
		//pEntity->m_pState = m_pSpriteViewer->GetState();

		pEntity->SetSprite( pSprite );

		pEntity->Spawn();

		m_pSpriteViewer->GetState()->SetEntity( pEntity );
	}
	else
	{
		delete pSprite;
	}

	InitializeUI();

	return true;
}

void CMainPanel::FreeSprite()
{
	m_p3DView->PrepareForLoad();

	m_pSpriteViewer->GetState()->ClearEntity();
}

void CMainPanel::InitializeUI()
{
	//ForEachPanel( &CBaseControlPanel::InitializeUI );
}

void CMainPanel::PageChanged( wxBookCtrlEvent& event )
{
	const int iOldIndex = event.GetOldSelection();
	const int iNewIndex = event.GetSelection();

	/*
	if( iOldIndex != wxNOT_FOUND )
	{
		CBaseControlPanel* const pPage = static_cast<CBaseControlPanel*>( m_pControlPanels->GetPage( iOldIndex ) );

		pPage->PanelDeactivated();
	}

	if( iNewIndex != wxNOT_FOUND )
	{
		CBaseControlPanel* const pPage = static_cast<CBaseControlPanel*>( m_pControlPanels->GetPage( iNewIndex ) );

		pPage->PanelActivated();
	}
	*/
}

bool CMainPanel::LoadBackgroundTexture( const wxString& szFilename )
{
	return m_p3DView->LoadBackgroundTexture( szFilename );
}

void CMainPanel::UnloadBackgroundTexture()
{
	return m_p3DView->UnloadBackgroundTexture();
}

void CMainPanel::TakeScreenshot()
{
	m_p3DView->TakeScreenshot();
}
}