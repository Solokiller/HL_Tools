#include "common/Logging.h"
#include "common/CGlobals.h"

#include "soundsystem/CSoundSystem.h"

#include "cvar/CCVar.h"

#include "game/studiomodel/CStudioModel.h"

#include "game/studiomodel/CStudioModelRenderer.h"

#include "CStudioModelEntity.h"

static cvar::CCVar s_ent_playsounds( "s_ent_playsounds", cvar::CCVarArgsBuilder().FloatValue( 0 ).HelpInfo( "Whether or not to play sounds triggered by animation events" ) );

LINK_ENTITY_TO_CLASS( studiomodel, CStudioModelEntity );

void CStudioModelEntity::OnCreate()
{
	BaseClass::OnCreate();

	SetThink( &ThisClass::AnimThink );
}

void CStudioModelEntity::OnDestroy()
{
	//TODO: once models are managed by a cache, don't do this
	delete m_pModel;

	BaseClass::OnDestroy();
}

bool CStudioModelEntity::Spawn()
{
	SetSequence( 0 );
	SetController( 0, 0.0f );
	SetController( 1, 0.0f );
	SetController( 2, 0.0f );
	SetController( 3, 0.0f );
	SetMouth( 0.0f );

	const studiohdr_t* pStudioHdr = m_pModel->GetStudioHeader();

	for( int n = 0; n < pStudioHdr->numbodyparts; ++n )
		SetBodygroup( n, 0 );

	SetSkin( 0 );

	return true;
}

void CStudioModelEntity::Draw( entity::DrawFlags_t flags )
{
	studiomodel::renderer().DrawModel( this, ( flags & entity::DRAWF_WIREFRAME_ONLY ) != 0 );
}

float CStudioModelEntity::AdvanceFrame( float dt )
{
	if( !m_pModel )
		return 0.0;

	const studiohdr_t* pStudioHdr = m_pModel->GetStudioHeader();

	const mstudioseqdesc_t* pseqdesc = pStudioHdr->GetSequence( m_iSequence );

	if( dt == 0.0 )
	{
		dt = ( Globals.GetCurrentTime() - m_flAnimTime );
		if( dt <= 0.001 )
		{
			m_flAnimTime = Globals.GetCurrentTime();
			return 0.0;
		}
	}

	if( !m_flAnimTime )
		dt = 0.0;

	/*
	if( dt > 0.1f )
	dt = 0.1f;
	*/

	m_flFrame += dt * pseqdesc->fps * m_flFrameRate;

	if( pseqdesc->numframes <= 1 )
	{
		m_flFrame = 0;
	}
	else
	{
		// wrap
		m_flFrame -= ( int ) ( m_flFrame / ( pseqdesc->numframes - 1 ) ) * ( pseqdesc->numframes - 1 );
	}

	m_flAnimTime = Globals.GetCurrentTime();

	return dt;
}

void CStudioModelEntity::HandleAnimEvent( const CAnimEvent& event )
{
	//TODO: move to subclass.
	switch( event.iEvent )
	{
	case SCRIPT_EVENT_SOUND:			// Play a named wave file
	case SCRIPT_EVENT_SOUND_VOICE:
	case SCRIPT_CLIENT_EVENT_SOUND:
		{
			if( s_ent_playsounds.GetBool() )
			{
				soundSystem().PlaySound( event.pszOptions, 1.0f, soundsystem::PITCH_NORM );
			}

			break;
		}

	default: break;
	}
}

int CStudioModelEntity::GetAnimationEvent( CAnimEvent& event, float flStart, float flEnd, int index, const bool bAllowClientEvents )
{
	if( !m_pModel )
		return 0;

	const studiohdr_t* pStudioHdr = m_pModel->GetStudioHeader();

	if( m_iSequence >= pStudioHdr->numseq )
		return 0;

	int events = 0;

	const mstudioseqdesc_t* pseqdesc = pStudioHdr->GetSequence( m_iSequence );
	const mstudioevent_t* pevent = ( const mstudioevent_t * ) ( ( const byte * ) pStudioHdr + pseqdesc->eventindex );

	if( pseqdesc->numevents == 0 || index > pseqdesc->numevents )
		return 0;

	if( pseqdesc->numframes <= 1 )
	{
		flStart = 0;
		flEnd = 1.0;
	}

	for( ; index < pseqdesc->numevents; index++ )
	{
		//TODO: maybe leave it up to the listener to filter these out?
		if( !bAllowClientEvents )
		{
			// Don't send client-side events to the server AI
			if( pevent[ index ].event >= EVENT_CLIENT )
				continue;
		}

		if( ( pevent[ index ].frame >= flStart && pevent[ index ].frame < flEnd ) ||
			( ( pseqdesc->flags & STUDIO_LOOPING ) && flEnd >= pseqdesc->numframes - 1 && pevent[ index ].frame < flEnd - pseqdesc->numframes + 1 ) )
		{
			event.iEvent = pevent[ index ].event;
			event.pszOptions = pevent[ index ].options;
			return index + 1;
		}
	}
	return 0;
}

void CStudioModelEntity::DispatchAnimEvents( const bool bAllowClientEvents, float flInterval )
{
	if( !m_pModel )
	{
		Message( "Gibbed monster is thinking!\n" );
		return;
	}

	const studiohdr_t* pStudioHdr = m_pModel->GetStudioHeader();

	// FIXME: I have to do this or some events get missed, and this is probably causing the problem below
	//This isn't really necessary, at least not in a tool.
	//flInterval = 0.1f;

	const mstudioseqdesc_t* pseqdesc = pStudioHdr->GetSequence( m_iSequence );

	// FIX: this still sometimes hits events twice
	float flStart = m_flFrame + ( m_flLastEventCheck - m_flAnimTime ) * pseqdesc->fps * m_flFrameRate;
	float flEnd = m_flFrame + flInterval * pseqdesc->fps * m_flFrameRate;
	m_flLastEventCheck = m_flAnimTime + flInterval;

	CAnimEvent event;

	int index = 0;

	while( ( index = GetAnimationEvent( event, flStart, flEnd, index, bAllowClientEvents ) ) != 0 )
	{
		HandleAnimEvent( event );
	}
}

void CStudioModelEntity::AnimThink()
{
	const float flTime = AdvanceFrame();

	DispatchAnimEvents( true, flTime );
}

int CStudioModelEntity::SetFrame( const int iFrame )
{
	if( iFrame == -1 )
		return static_cast<int>( m_flFrame );

	if( !m_pModel )
		return 0;

	mstudioseqdesc_t* pseqdesc = m_pModel->GetStudioHeader()->GetSequence( m_iSequence );

	m_flFrame = static_cast<float>( iFrame );

	if( pseqdesc->numframes <= 1 )
	{
		m_flFrame = 0;
	}
	else
	{
		// wrap
		m_flFrame -= ( int ) ( m_flFrame / ( pseqdesc->numframes - 1 ) ) * ( pseqdesc->numframes - 1 );
	}

	m_flAnimTime = Globals.GetCurrentTime();

	return static_cast<int>( m_flFrame );
}

void CStudioModelEntity::SetModel( studiomodel::CStudioModel* pModel )
{
	//TODO: release old model.
	m_pModel = pModel;
}

int CStudioModelEntity::GetNumFrames() const
{
	const mstudioseqdesc_t* const pseqdesc = m_pModel->GetStudioHeader()->GetSequence( m_iSequence );

	return pseqdesc->numframes;
}

int CStudioModelEntity::SetSequence( const int iSequence )
{
	if( iSequence > m_pModel->GetStudioHeader()->numseq )
		return m_iSequence;

	m_iSequence = iSequence;
	m_flFrame = 0;

	return m_iSequence;
}

void CStudioModelEntity::GetSequenceInfo( float& flFrameRate, float& flGroundSpeed ) const
{
	const mstudioseqdesc_t* pseqdesc = m_pModel->GetStudioHeader()->GetSequence( m_iSequence );

	if( pseqdesc->numframes > 1 )
	{
		flFrameRate = pseqdesc->fps;
		flGroundSpeed = static_cast<float>( pseqdesc->linearmovement.length() );
		flGroundSpeed = flGroundSpeed * pseqdesc->fps / ( pseqdesc->numframes - 1 );
	}
	else
	{
		flFrameRate = 256.0;
		flGroundSpeed = 0.0;
	}
}

int CStudioModelEntity::SetBodygroup( const int iBodygroup, const int iValue )
{
	if( !m_pModel )
		return 0;

	if( iBodygroup > m_pModel->GetStudioHeader()->numbodyparts )
		return -1;

	if( m_pModel->CalculateBodygroup( iBodygroup, iValue, m_iBodygroup ) )
		return iValue;

	return -1;
}

int CStudioModelEntity::SetSkin( const int iSkin )
{
	if( !m_pModel )
		return 0;

	if( iSkin < m_pModel->GetStudioHeader()->numskinfamilies )
	{
		m_iSkin = iSkin;
	}

	return m_iSkin;
}

byte CStudioModelEntity::GetControllerByIndex( const int iController ) const
{
	assert( iController >= 0 && iController < STUDIO_MAX_CONTROLLERS );

	return m_uiController[ iController ];
}

float CStudioModelEntity::GetControllerValue( const int iController ) const
{
	if( !m_pModel )
		return 0.0f;

	if( iController < 0 || iController >= STUDIO_TOTAL_CONTROLLERS )
		return 0;

	const studiohdr_t* pStudioHdr = m_pModel->GetStudioHeader();

	const mstudiobonecontroller_t* pbonecontroller = pStudioHdr->GetBoneControllers();

	// find first controller that matches the index
	int i;

	for( i = 0; i < pStudioHdr->numbonecontrollers; ++i, ++pbonecontroller )
	{
		if( pbonecontroller->index == iController )
			break;
	}

	if( i >= pStudioHdr->numbonecontrollers )
		return 0;

	byte uiValue;

	if( iController == STUDIO_MOUTH_CONTROLLER )
		uiValue = m_uiMouth;
	else
		uiValue = m_uiController[ iController ];

	return static_cast<float>( uiValue * ( 1.0 / 255.0 ) * ( pbonecontroller->end - pbonecontroller->start ) + pbonecontroller->start );
}

float CStudioModelEntity::SetController( const int iController, float flValue )
{
	if( !m_pModel )
		return 0.0f;

	const studiohdr_t* pStudioHdr = m_pModel->GetStudioHeader();

	const mstudiobonecontroller_t* pbonecontroller = pStudioHdr->GetBoneControllers();

	// find first controller that matches the index
	int i;

	for( i = 0; i < pStudioHdr->numbonecontrollers; ++i, ++pbonecontroller )
	{
		if( pbonecontroller->index == iController )
			break;
	}

	if( i >= pStudioHdr->numbonecontrollers )
		return flValue;

	// wrap 0..360 if it's a rotational controller
	if( pbonecontroller->type & ( STUDIO_XR | STUDIO_YR | STUDIO_ZR ) )
	{
		// ugly hack, invert value if end < start
		if( pbonecontroller->end < pbonecontroller->start )
			flValue = -flValue;

		// does the controller not wrap?
		if( pbonecontroller->start + 359.0 >= pbonecontroller->end )
		{
			if( flValue >( ( pbonecontroller->start + pbonecontroller->end ) / 2.0 ) + 180 )
				flValue = flValue - 360;
			if( flValue < ( ( pbonecontroller->start + pbonecontroller->end ) / 2.0 ) - 180 )
				flValue = flValue + 360;
		}
		else
		{
			if( flValue > 360 )
				flValue = static_cast<float>( flValue - ( int ) ( flValue / 360.0 ) * 360.0 );
			else if( flValue < 0 )
				flValue = static_cast<float>( flValue + ( int ) ( ( flValue / -360.0 ) + 1 ) * 360.0 );
		}
	}

	int setting = ( int ) ( 255 * ( flValue - pbonecontroller->start ) /
		( pbonecontroller->end - pbonecontroller->start ) );

	if( setting < 0 ) setting = 0;
	if( setting > 255 ) setting = 255;
	m_uiController[ iController ] = setting;

	return static_cast<float>( setting * ( 1.0 / 255.0 ) * ( pbonecontroller->end - pbonecontroller->start ) + pbonecontroller->start );
}

float CStudioModelEntity::SetMouth( float flValue )
{
	if( !m_pModel )
		return 0.0f;

	const studiohdr_t* pStudioHdr = m_pModel->GetStudioHeader();

	const mstudiobonecontroller_t* pbonecontroller = pStudioHdr->GetBoneControllers();

	// find first controller that matches the mouth
	for( int i = 0; i < pStudioHdr->numbonecontrollers; ++i, ++pbonecontroller )
	{
		if( pbonecontroller->index == STUDIO_MOUTH_CONTROLLER )
			break;
	}

	// wrap 0..360 if it's a rotational controller
	if( pbonecontroller->type & ( STUDIO_XR | STUDIO_YR | STUDIO_ZR ) )
	{
		// ugly hack, invert value if end < start
		if( pbonecontroller->end < pbonecontroller->start )
			flValue = -flValue;

		// does the controller not wrap?
		if( pbonecontroller->start + 359.0 >= pbonecontroller->end )
		{
			if( flValue >( ( pbonecontroller->start + pbonecontroller->end ) / 2.0 ) + 180 )
				flValue = flValue - 360;
			if( flValue < ( ( pbonecontroller->start + pbonecontroller->end ) / 2.0 ) - 180 )
				flValue = flValue + 360;
		}
		else
		{
			if( flValue > 360 )
				flValue = static_cast<float>( flValue - ( int ) ( flValue / 360.0 ) * 360.0 );
			else if( flValue < 0 )
				flValue = static_cast<float>( flValue + ( int ) ( ( flValue / -360.0 ) + 1 ) * 360.0 );
		}
	}

	int setting = ( int ) ( 64 * ( flValue - pbonecontroller->start ) / ( pbonecontroller->end - pbonecontroller->start ) );

	if( setting < 0 ) setting = 0;
	if( setting > 64 ) setting = 64;
	m_uiMouth = setting;

	return static_cast<float>( setting * ( 1.0 / 64.0 ) * ( pbonecontroller->end - pbonecontroller->start ) + pbonecontroller->start );
}

byte CStudioModelEntity::GetBlendingByIndex( const int iBlender ) const
{
	assert( iBlender >= 0 && iBlender < STUDIO_MAX_BLENDERS );

	return m_uiBlending[ iBlender ];
}

float CStudioModelEntity::GetBlendingValue( const int iBlender ) const
{
	if( !m_pModel )
		return 0.0f;

	if( iBlender < 0 || iBlender >= STUDIO_MAX_BLENDERS )
		return 0;

	const studiohdr_t* pStudioHdr = m_pModel->GetStudioHeader();

	const mstudioseqdesc_t* pseqdesc = pStudioHdr->GetSequence( m_iSequence );

	if( pseqdesc->blendtype[ iBlender ] == 0 )
		return 0;

	return static_cast<float>( m_uiBlending[ iBlender ] * ( 1.0 / 255.0 ) * ( pseqdesc->blendend[ iBlender ] - pseqdesc->blendstart[ iBlender ] ) + pseqdesc->blendstart[ iBlender ] );
}

float CStudioModelEntity::SetBlending( const int iBlender, float flValue )
{
	if( !m_pModel )
		return 0.0f;

	if( iBlender < 0 || iBlender >= STUDIO_MAX_BLENDERS )
		return 0;

	const studiohdr_t* pStudioHdr = m_pModel->GetStudioHeader();

	const mstudioseqdesc_t* pseqdesc = pStudioHdr->GetSequence( m_iSequence );

	if( pseqdesc->blendtype[ iBlender ] == 0 )
		return flValue;

	if( pseqdesc->blendtype[ iBlender ] & ( STUDIO_XR | STUDIO_YR | STUDIO_ZR ) )
	{
		// ugly hack, invert value if end < start
		if( pseqdesc->blendend[ iBlender ] < pseqdesc->blendstart[ iBlender ] )
			flValue = -flValue;

		// does the controller not wrap?
		if( pseqdesc->blendstart[ iBlender ] + 359.0 >= pseqdesc->blendend[ iBlender ] )
		{
			if( flValue > ( ( pseqdesc->blendstart[ iBlender ] + pseqdesc->blendend[ iBlender ] ) / 2.0 ) + 180 )
				flValue = flValue - 360;
			if( flValue < ( ( pseqdesc->blendstart[ iBlender ] + pseqdesc->blendend[ iBlender ] ) / 2.0 ) - 180 )
				flValue = flValue + 360;
		}
	}

	int setting = ( int ) ( 255 * ( flValue - pseqdesc->blendstart[ iBlender ] ) / ( pseqdesc->blendend[ iBlender ] - pseqdesc->blendstart[ iBlender ] ) );

	if( setting < 0 ) setting = 0;
	if( setting > 255 ) setting = 255;

	m_uiBlending[ iBlender ] = setting;

	return static_cast<float>( setting * ( 1.0 / 255.0 ) * ( pseqdesc->blendend[ iBlender ] - pseqdesc->blendstart[ iBlender ] ) + pseqdesc->blendstart[ iBlender ] );
}

void CStudioModelEntity::ExtractBbox( glm::vec3& vecMins, glm::vec3& vecMaxs ) const
{
	const mstudioseqdesc_t* pseqdesc = m_pModel->GetStudioHeader()->GetSequence( m_iSequence );

	vecMins = pseqdesc->bbmin;
	vecMaxs = pseqdesc->bbmax;
}

mstudiomodel_t* CStudioModelEntity::GetModelByBodyPart( const int iBodyPart ) const
{
	return m_pModel->GetModelByBodyPart( m_iBodygroup, iBodyPart );
}

CStudioModelEntity::MeshList_t CStudioModelEntity::ComputeMeshList( const int iTexture ) const
{
	assert( m_pModel );

	MeshList_t meshes;

	const auto pStudioHdr = m_pModel->GetStudioHeader();

	const short* const pskinref = m_pModel->GetTextureHeader()->GetSkins();

	int iBodygroup = 0;

	for( int iBodyPart = 0; iBodyPart < pStudioHdr->numbodyparts; ++iBodyPart )
	{
		mstudiobodyparts_t *pbodypart = pStudioHdr->GetBodypart( iBodyPart );

		for( int iModel = 0; iModel < pbodypart->nummodels; ++iModel )
		{
			m_pModel->CalculateBodygroup( iBodyPart, iModel, iBodygroup );

			mstudiomodel_t* pModel = m_pModel->GetModelByBodyPart( iBodygroup, iBodyPart );

			for( int iMesh = 0; iMesh < pModel->nummesh; ++iMesh )
			{
				const mstudiomesh_t* pMesh = ( ( const mstudiomesh_t* ) ( ( const byte* ) m_pModel->GetStudioHeader() + pModel->meshindex ) ) + iMesh;

				if( pskinref[ pMesh->skinref ] == iTexture )
				{
					meshes.push_back( pMesh );
				}
			}
		}
	}

	return meshes;
}