#ifndef HLMV_CHLMV_H
#define HLMV_CHLMV_H

#include "tools/shared/CBaseTool.h"

#include "../settings/CHLMVSettings.h"

namespace hlmv
{
class CHLMVState;

class CMainWindow;
class CFullscreenWindow;

/**
*	Facade class to access the entire HLMV program.
*/
class CHLMV final : public tools::CBaseTool
{
public:
	/**
	*	Constructor.
	*/
	CHLMV();

	/**
	*	Destructor.
	*/
	~CHLMV();

protected:
	settings::CBaseSettings* CreateSettings() override final;

	bool PostInitialize() override final;

	void PreShutdown() override final;

	void RunFrame() override final;

	void OnExit( const bool bMainWndClosed ) override final;

public:
	/**
	*	Gets the settings object.
	*/
	const CHLMVSettings* GetSettings() const { return static_cast<const CHLMVSettings*>( CBaseTool::GetSettings() ); }

	/**
	*	@copydoc GetSettings() const
	*/
	CHLMVSettings* GetSettings() { return static_cast<CHLMVSettings*>( CBaseTool::GetSettings() ); }

	/**
	*	Gets the state object.
	*/
	const CHLMVState* GetState() const { return m_pState; }

	/**
	*	@copydoc GetState() const
	*/
	CHLMVState* GetState() { return m_pState; }

	/**
	*	Gets the main window.
	*/
	CMainWindow* GetMainWindow() { return m_pMainWindow; }

	/**
	*	Sets the main window.
	*/
	void SetMainWindow( CMainWindow* const pMainWindow );

	/**
	*	Gets the fullscreen window.
	*/
	CFullscreenWindow* GetFullscreenWindow() { return m_pFullscreenWindow; }

	/**
	*	Sets the fullscreen window.
	*/
	void SetFullscreenWindow( CFullscreenWindow* const pWindow );

	//Load/Save model
	/**
	*	Loads a model with the given filename.
	*	@param pszFilename Model filename.
	*	@return true if the model was successfully loaded, false otherwise.
	*/
	bool LoadModel( const char* const pszFilename );

	/**
	*	Prompts the user to load a model.
	*	@return true if the user elected to load a model, and the model was successfully loaded, false otherwise.
	*/
	bool PromptLoadModel();

	/**
	*	Saves the current model with the given filename.
	*	@param pszFilename Filename to save the model as.
	*	@return true if a model is currently loaded, and the model was successfully saved.
	*/
	bool SaveModel( const char* const pszFilename );

	/**
	*	Prompts the user to save the current model.
	*	@return true if the user elected to save the model, and the model was successfully saved, false otherwise.
	*/
	bool PromptSaveModel();

	//Background and Ground textures
	/**
	*	Loads a background texture, replacing the current background texture, if one is present.
	*	@param pszFilename Filename of the texture to load.
	*	@return true if the texture was successfully loaded, false otherwise.
	*/
	bool LoadBackgroundTexture( const char* const pszFilename );

	/**
	*	Prompts the user to load a background texture.
	*	@return true if the user elected to load a background texture, and the texture was successfully loaded, false otherwise.
	*/
	bool PromptLoadBackgroundTexture();

	/**
	*	Unloads the current background texture.
	*/
	void UnloadBackgroundTexture();

	/**
	*	Loads a ground texture, replacing the current ground texture, if one is present.
	*	@param pszFilename Filename of the texture to load.
	*	@return true if the texture was successfully loaded, false otherwise.
	*/
	bool LoadGroundTexture( const char* const pszFilename );

	/**
	*	Prompts the user to load a ground texture.
	*	@return true if the user elected to load a ground texture, and the texture was successfully loaded, false otherwise.
	*/
	bool PromptGroundTexture();

	/**
	*	Unloads the current ground texture.
	*/
	void UnloadGroundTexture();

	/**
	*	Saves the given texture's UV map to the given file.
	*/
	void SaveUVMap( const wxString& szFilename, const int iTexture );

private:
	CHLMVState* m_pState;

	CMainWindow* m_pMainWindow = nullptr;
	CFullscreenWindow* m_pFullscreenWindow = nullptr;

private:
	CHLMV( const CHLMV& ) = delete;
	CHLMV& operator=( const CHLMV& ) = delete;
};
}

#endif //HLMV_CHLMV_H