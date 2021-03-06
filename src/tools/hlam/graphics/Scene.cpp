#include <cassert>
#include <cmath>
#include <utility>

#include <GL/glew.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include "core/shared/WorldTime.hpp"

#include "engine/renderer/sprite/SpriteRenderer.hpp"
#include "engine/renderer/studiomodel/StudioModelRenderer.hpp"
#include "engine/shared/renderer/studiomodel/IStudioModelRenderer.hpp"
#include "entity/HLMVStudioModelEntity.hpp"

#include "game/entity/BaseEntityList.hpp"
#include "game/entity/EntityManager.hpp"

#include "graphics/GraphicsUtils.hpp"
#include "graphics/IGraphicsContext.hpp"
#include "graphics/Scene.hpp"

#include "utility/Color.hpp"

namespace graphics
{
static const int CROSSHAIR_LINE_WIDTH = 3;
static const int CROSSHAIR_LINE_START = 5;
static const int CROSSHAIR_LINE_LENGTH = 10;
static const int CROSSHAIR_LINE_END = CROSSHAIR_LINE_START + CROSSHAIR_LINE_LENGTH;

static const int GUIDELINES_LINE_WIDTH = 1;
static const int GUIDELINES_LINE_LENGTH = 5;
static const int GUIDELINES_POINT_LINE_OFFSET = 2;
static const int GUIDELINES_OFFSET = GUIDELINES_LINE_LENGTH + (GUIDELINES_POINT_LINE_OFFSET * 2) + GUIDELINES_LINE_WIDTH;

static const int GUIDELINES_EDGE_WIDTH = 4;

Scene::Scene(TextureLoader* textureLoader, soundsystem::ISoundSystem* soundSystem, WorldTime* worldTime)
	: _textureLoader(textureLoader)
	, _spriteRenderer(std::make_unique<sprite::SpriteRenderer>(worldTime))
	, _studioModelRenderer(std::make_unique<studiomdl::StudioModelRenderer>())
	, _worldTime(worldTime)
	//Use the default list class for now
	, _entityManager(std::make_unique<EntityManager>(std::make_unique<BaseEntityList>(), _worldTime))
	, _entityContext(std::make_unique<EntityContext>(_worldTime,
		_studioModelRenderer.get(), _spriteRenderer.get(),
		_entityManager->GetEntityList(), _entityManager.get(),
		soundSystem))
{
	assert(_textureLoader);

	SetCurrentCamera(nullptr);
}

Scene::~Scene()
{
	if (BackgroundTexture != 0)
	{
		glDeleteTextures(1, &BackgroundTexture);
	}

	if (GroundTexture != 0)
	{
		glDeleteTextures(1, &GroundTexture);
	}
}

void Scene::SetGraphicsContext(std::unique_ptr<IGraphicsContext>&& graphicsContext)
{
	_graphicsContext = std::move(graphicsContext);
}

glm::vec3 Scene::GetLightColor() const
{
	return _studioModelRenderer->GetLightColor();
}

void Scene::SetLightColor(const glm::vec3& value)
{
	_studioModelRenderer->SetLightColor(value);
}

glm::vec3 Scene::GetWireframeColor() const
{
	return _studioModelRenderer->GetWireframeColor();
}

void Scene::SetWireframeColor(const glm::vec3& value)
{
	_studioModelRenderer->SetWireframeColor(value);
}

void Scene::AlignOnGround()
{
	auto entity = GetEntity();

	auto model = entity->GetEditableModel();

	//First try finding the idle sequence, since that typically represents a model "at rest"
	//Failing that, use the first sequence
	auto idleFinder = [&]() -> const studiomdl::Sequence*
	{
		if (model->Sequences.empty())
		{
			return nullptr;
		}

		for (const auto& sequence : model->Sequences)
		{
			if (sequence->Label == "idle")
			{
				return sequence.get();
			}
		}

		return model->Sequences[0].get();
	};

	auto sequence = idleFinder();

	entity->SetOrigin({0, 0, sequence ? -sequence->BBMin.z : 0});
}

void Scene::Initialize()
{
	++_initializeCount;

	if (_initializeCount != 1)
	{
		return;
	}

	if (!_studioModelRenderer->Initialize())
	{
		//TODO: handle error
	}

	glGenTextures(1, &UVMeshTexture);
}

void Scene::Shutdown()
{
	--_initializeCount;

	if (_initializeCount != 0)
	{
		return;
	}

	glDeleteTexture(UVMeshTexture);
	UVMeshTexture = 0;

	_studioModelRenderer->Shutdown();
}

void Scene::Tick()
{
	_entityManager->RunFrame();
}

void Scene::Draw()
{
	//TODO: really ugly, needs reworking
	if (nullptr != _entity)
	{
		auto model = _entity->GetEditableModel();
		
		if (model->TexturesNeedCreating)
		{
			model->TexturesNeedCreating = false;
			model->CreateTextures(*_textureLoader);
		}
	}

	glClearColor(BackgroundColor.r, BackgroundColor.g, BackgroundColor.b, 1.0f);

	if (MirrorOnGround)
	{
		glClearStencil(0);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}
	else
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glViewport(0, 0, _windowWidth, _windowHeight);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	_drawnPolygonsCount = 0;

	if (ShowTexture)
	{
		DrawTexture(TextureXOffset, TextureYOffset, _windowWidth, _windowHeight, _entity, TextureIndex, TextureScale, ShowUVMap, OverlayUVMap);
	}
	else
	{
		DrawModel();
	}

	const int centerX = _windowWidth / 2;
	const int centerY = _windowHeight / 2;

	if (ShowCrosshair)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glOrtho(0.0f, (float)_windowWidth, (float)_windowHeight, 0.0f, 1.0f, -1.0f);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glDisable(GL_CULL_FACE);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		glDisable(GL_TEXTURE_2D);

		glColor4fv(glm::value_ptr(glm::vec4{CrosshairColor, 1}));

		glPointSize(CROSSHAIR_LINE_WIDTH);
		glLineWidth(CROSSHAIR_LINE_WIDTH);

		glBegin(GL_POINTS);

		glVertex2f(centerX - CROSSHAIR_LINE_WIDTH / 2, centerY + 1);

		glEnd();

		glBegin(GL_LINES);

		glVertex2f(centerX - CROSSHAIR_LINE_START, centerY);
		glVertex2f(centerX - CROSSHAIR_LINE_END, centerY);

		glVertex2f(centerX + CROSSHAIR_LINE_START, centerY);
		glVertex2f(centerX + CROSSHAIR_LINE_END, centerY);

		glVertex2f(centerX, centerY - CROSSHAIR_LINE_START);
		glVertex2f(centerX, centerY - CROSSHAIR_LINE_END);

		glVertex2f(centerX, centerY + CROSSHAIR_LINE_START);
		glVertex2f(centerX, centerY + CROSSHAIR_LINE_END);

		glEnd();

		glPointSize(1);
		glLineWidth(1);

		glPopMatrix();
	}

	if (ShowGuidelines)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glOrtho(0.0f, (float)_windowWidth, (float)_windowHeight, 0.0f, 1.0f, -1.0f);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glDisable(GL_CULL_FACE);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		glDisable(GL_TEXTURE_2D);

		glColor4fv(glm::value_ptr(glm::vec4{CrosshairColor, 1}));

		glPointSize(GUIDELINES_LINE_WIDTH);
		glLineWidth(GUIDELINES_LINE_WIDTH);

		glBegin(GL_POINTS);

		for (int yPos = _windowHeight - GUIDELINES_LINE_LENGTH; yPos >= centerY + CROSSHAIR_LINE_END; yPos -= GUIDELINES_OFFSET)
		{
			glVertex2f(centerX - GUIDELINES_LINE_WIDTH, yPos);
		}

		glEnd();

		glBegin(GL_LINES);

		for (int yPos = _windowHeight - GUIDELINES_LINE_LENGTH - GUIDELINES_POINT_LINE_OFFSET - GUIDELINES_LINE_WIDTH;
			yPos >= centerY + CROSSHAIR_LINE_END + GUIDELINES_LINE_LENGTH;
			yPos -= GUIDELINES_OFFSET)
		{
			glVertex2f(centerX, yPos);
			glVertex2f(centerX, yPos - GUIDELINES_LINE_LENGTH);
		}

		glEnd();

		const float flWidth = _windowHeight * (16 / 9.0);

		glLineWidth(GUIDELINES_EDGE_WIDTH);

		glBegin(GL_LINES);

		glVertex2f((_windowWidth / 2.) - (flWidth / 2), 0);
		glVertex2f((_windowWidth / 2.) - (flWidth / 2), _windowHeight);

		glVertex2f((_windowWidth / 2.) + (flWidth / 2), 0);
		glVertex2f((_windowWidth / 2.) + (flWidth / 2), _windowHeight);

		glEnd();

		glPointSize(1);
		glLineWidth(1);

		glPopMatrix();
	}
}

void Scene::SetupRenderMode(RenderMode renderMode)
{
	if (renderMode == RenderMode::INVALID)
		renderMode = CurrentRenderMode;

	graphics::SetupRenderMode(renderMode, EnableBackfaceCulling);
}

void Scene::DrawModel()
{
	auto camera = GetCurrentCamera();

	//
	// draw background
	//

	if (ShowBackground && BackgroundTexture != GL_INVALID_TEXTURE_ID && !ShowTexture)
	{
		graphics::DrawBackground(BackgroundTexture);
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glLoadMatrixf(glm::value_ptr(camera->GetProjectionMatrix()));

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glLoadMatrixf(glm::value_ptr(camera->GetViewMatrix()));

	if (ShowAxes)
	{
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_DEPTH_TEST);

		const float flLength = 50.0f;

		glLineWidth(1.0f);

		glBegin(GL_LINES);

		glColor3f(1.0f, 0, 0);

		glVertex3f(0, 0, 0);
		glVertex3f(flLength, 0, 0);

		glColor3f(0, 1, 0);

		glVertex3f(0, 0, 0);
		glVertex3f(0, flLength, 0);

		glColor3f(0, 0, 1.0f);

		glVertex3f(0, 0, 0);
		glVertex3f(0, 0, flLength);

		glEnd();
	}

	_studioModelRenderer->SetViewerOrigin(camera->GetOrigin());
	_studioModelRenderer->SetViewerRight(camera->GetRightVector());

	const unsigned int uiOldPolys = _studioModelRenderer->GetDrawnPolygonsCount();

	if (nullptr != _entity)
	{
		// setup stencil buffer and draw mirror
		if (MirrorOnGround)
		{
			graphics::DrawMirroredModel(*_studioModelRenderer, _entity,
				CurrentRenderMode,
				ShowWireframeOverlay,
				FloorLength,
				EnableBackfaceCulling);
		}
	}

	SetupRenderMode();

	if (nullptr != _entity)
	{
		const glm::vec3& vecScale = _entity->GetScale();

		//Determine if an odd number of scale values are negative. The cull face has to be changed if so.
		const float flScale = vecScale.x * vecScale.y * vecScale.z;

		glCullFace(flScale > 0 ? GL_FRONT : GL_BACK);

		renderer::DrawFlags flags = renderer::DrawFlag::NONE;

		if (ShowWireframeOverlay)
		{
			flags |= renderer::DrawFlag::WIREFRAME_OVERLAY;
		}

		if (CameraIsFirstPerson)
		{
			flags |= renderer::DrawFlag::IS_VIEW_MODEL;
		}

		if (DrawShadows)
		{
			flags |= renderer::DrawFlag::DRAW_SHADOWS;
		}

		if (FixShadowZFighting)
		{
			flags |= renderer::DrawFlag::FIX_SHADOW_Z_FIGHTING;
		}

		//TODO: these should probably be made separate somehow
		if (ShowHitboxes)
		{
			flags |= renderer::DrawFlag::DRAW_HITBOXES;
		}

		if (ShowBones)
		{
			flags |= renderer::DrawFlag::DRAW_BONES;
		}

		if (ShowAttachments)
		{
			flags |= renderer::DrawFlag::DRAW_ATTACHMENTS;
		}

		if (ShowEyePosition)
		{
			flags |= renderer::DrawFlag::DRAW_EYE_POSITION;
		}

		if (ShowNormals)
		{
			flags |= renderer::DrawFlag::DRAW_NORMALS;
		}

		_entity->Draw(flags);

		auto renderInfo = _entity->GetRenderInfo();

		//TODO: this is a temporary hack. The graphics scene architecture needs a complete overhaul first,
		//then this can be done by rendering the model in a separate viewmodel layer
		if (CameraIsFirstPerson)
		{
			renderInfo.Origin.z -= 1;
		}

		if (DrawSingleBoneIndex != -1)
		{
			_entityContext->StudioModelRenderer->DrawSingleBone(renderInfo, DrawSingleBoneIndex);
		}

		if (DrawSingleAttachmentIndex != -1)
		{
			_entityContext->StudioModelRenderer->DrawSingleAttachment(renderInfo, DrawSingleAttachmentIndex);
		}

		if (DrawSingleHitboxIndex != -1)
		{
			_entityContext->StudioModelRenderer->DrawSingleHitbox(renderInfo, DrawSingleHitboxIndex);
		}
	}

	//
	// draw ground
	//

	if (ShowGround)
	{
		glm::vec2 textureOffset{0};

		//Calculate texture offset based on sequence movement and current frame
		if (_entity)
		{
			auto model = _entity->GetEditableModel();

			const int sequenceIndex = _entity->GetSequence();

			if (sequenceIndex >= 0 && sequenceIndex < model->Sequences.size())
			{
				const auto& sequence = *model->Sequences[_entity->GetSequence()];

				//Scale offset to current frame
				const float currentFrame = _entity->GetFrame() / (sequence.NumFrames - 1);

				float delta;

				if (currentFrame >= _previousFloorFrame)
				{
					delta = currentFrame - _previousFloorFrame;
				}
				else
				{
					delta = (currentFrame + 1) - _previousFloorFrame;
				}

				_previousFloorFrame = currentFrame;

				//Adjust scrolling direction based on whether the model is mirrored, but don't apply scale itself
				const int xDirection = _entity->GetScale().x > 0 ? 1 : -1;
				const int yDirection = _entity->GetScale().y > 0 ? 1 : -1;

				textureOffset.x = sequence.LinearMovement.x * delta * xDirection;
				textureOffset.y = -(sequence.LinearMovement.y * delta * yDirection);

				if (_floorSequence != _entity->GetSequence())
				{
					_floorSequence = _entity->GetSequence();
					_previousFloorFrame = 0;
					_floorTextureOffset.x = _floorTextureOffset.y = 0;
				}
			}
		}

		_floorTextureOffset += textureOffset;

		const float floorTextureLength = EnableFloorTextureTiling ? FloorTextureLength : FloorLength;

		//Prevent the offset from overflowing
		_floorTextureOffset.x = std::fmod(_floorTextureOffset.x, floorTextureLength);
		_floorTextureOffset.y = std::fmod(_floorTextureOffset.y, floorTextureLength);

		graphics::DrawFloor(FloorLength, floorTextureLength, _floorTextureOffset, GroundTexture, GroundColor, MirrorOnGround);
	}

	_drawnPolygonsCount = _studioModelRenderer->GetDrawnPolygonsCount() - uiOldPolys;

	if (ShowPlayerHitbox)
	{
		//Draw a transparent green box to display the player hitbox
		const glm::vec3 bbmin{-16, -16, 0};
		const glm::vec3 bbmax{16, 16, 72};

		auto v = CreateBoxFromBounds(bbmin, bbmax);

		DrawOutlinedBox(v, {0.0f, 1.0f, 0.0f, 0.5f}, {0.0f, 0.5f, 0.0f, 1.f});
	}

	if (ShowBBox)
	{
		if (_entity)
		{
			//Draw a transparent brownish box to display the bounding box
			auto model = _entity->GetEditableModel();

			const auto v = CreateBoxFromBounds(model->BoundingMin, model->BoundingMax);

			DrawOutlinedBox(v, {1.0f, 1.0f, 0.0f, 0.5f}, {0.5f, 0.5f, 0.0f, 1.0f});
		}
	}

	if (ShowCBox)
	{
		if (_entity)
		{
			//Draw a transparent orangeish box to display the clipping box
			auto model = _entity->GetEditableModel();

			const auto v = CreateBoxFromBounds(model->ClippingMin, model->ClippingMax);

			DrawOutlinedBox(v, {1.0f, 0.5f, 0.0f, 0.5f}, {0.5f, 0.25f, 0.0f, 1.0f});
		}
	}

	glPopMatrix();
}

void Scene::DrawTexture(const int xOffset, const int yOffset, const int width, const int height, StudioModelEntity* entity,
	const int textureIndex, const float textureScale, const bool showUVMap, const bool overlayUVMap)
{
	assert(entity);

	const auto model = entity->GetEditableModel();

	assert(model);

	if (textureIndex < 0 || textureIndex >= model->Textures.size())
	{
		return;
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0.0f, (float)width, (float)height, 0.0f, 1.0f, -1.0f);

	const auto& texture = *model->Textures[textureIndex];

	const float w = texture.Width * textureScale;
	const float h = texture.Height * textureScale;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	const float x = ((static_cast<float>(width) - w) / 2) + xOffset;
	const float y = ((static_cast<float>(height) - h) / 2) + yOffset;

	glDisable(GL_DEPTH_TEST);

	if (showUVMap && !overlayUVMap)
	{
		glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
		glDisable(GL_TEXTURE_2D);
		glRectf(x, y, x + w, y + h);
	}

	if (!showUVMap || overlayUVMap)
	{
		if (texture.Flags & STUDIO_NF_MASKED)
		{
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0.5f);
		}

		glEnable(GL_TEXTURE_2D);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBindTexture(GL_TEXTURE_2D, texture.TextureId);

		glBegin(GL_TRIANGLE_STRIP);

		glTexCoord2f(0, 0);
		glVertex2f(x, y);

		glTexCoord2f(1, 0);
		glVertex2f(x + w, y);

		glTexCoord2f(0, 1);
		glVertex2f(x, y + h);

		glTexCoord2f(1, 1);
		glVertex2f(x + w, y + h);

		glEnd();

		glBindTexture(GL_TEXTURE_2D, 0);

		if (texture.Flags & STUDIO_NF_MASKED)
		{
			glDisable(GL_ALPHA_TEST);
		}
	}

	if (showUVMap)
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.1f);

		glEnable(GL_TEXTURE_2D);
			
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBindTexture(GL_TEXTURE_2D, UVMeshTexture);

		glBegin(GL_TRIANGLE_STRIP);

		glTexCoord2f(0, 0);
		glVertex2f(x, y);

		glTexCoord2f(1, 0);
		glVertex2f(x + w, y);

		glTexCoord2f(0, 1);
		glVertex2f(x, y + h);

		glTexCoord2f(1, 1);
		glVertex2f(x + w, y + h);

		glEnd();

		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_ALPHA_TEST);
	}

	glPopMatrix();

	glClear(GL_DEPTH_BUFFER_BIT);
}
}
