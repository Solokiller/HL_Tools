#include <algorithm>
#include <utility>
#include <vector>

#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QMessageBox>
#include <QMetaEnum>
#include <QPainter>
#include <QSignalBlocker>
#include <QToolTip>

#include <glm/vec2.hpp>

#include "core/shared/Utility.hpp"

#include "entity/HLMVStudioModelEntity.hpp"

#include "graphics/GraphicsUtils.hpp"
#include "graphics/IGraphicsContext.hpp"
#include "graphics/Palette.hpp"
#include "graphics/TextureLoader.hpp"

#include "qt/QtUtilities.hpp"

#include "ui/StateSnapshot.hpp"

#include "ui/assets/studiomodel/StudioModelAsset.hpp"
#include "ui/assets/studiomodel/StudioModelUndoCommands.hpp"
#include "ui/assets/studiomodel/StudioModelValidators.hpp"
#include "ui/assets/studiomodel/dockpanels/StudioModelExportUVMeshDialog.hpp"
#include "ui/assets/studiomodel/dockpanels/StudioModelTexturesPanel.hpp"

#include "ui/settings/StudioModelSettings.hpp"

namespace ui::assets::studiomodel
{
static constexpr double TextureViewScaleMinimum = 0.1;
static constexpr double TextureViewScaleMaximum = 20;
static constexpr double TextureViewScaleDefault = 1;
static constexpr double TextureViewScaleSingleStepValue = 0.1;
static constexpr double TextureViewScaleSliderRatio = 10.0;
static constexpr double UVLineWidthSliderRatio = 10.0;

static int GetMeshIndexForDrawing(QComboBox* comboBox)
{
	int meshIndex = comboBox->currentIndex();

	if (comboBox->count() > 1 && meshIndex == (comboBox->count() - 1))
	{
		meshIndex = -1;
	}

	return meshIndex;
}

static QString FormatTextureName(const studiomdl::Texture& texture)
{
	return QString{"%1 (%2 x %3)"}.arg(texture.Name.c_str()).arg(texture.Width).arg(texture.Height);
}

StudioModelTexturesPanel::StudioModelTexturesPanel(StudioModelAsset* asset, QWidget* parent)
	: QWidget(parent)
	, _asset(asset)
{
	_ui.setupUi(this);

	const auto textureNameValidator = new UniqueTextureNameValidator(MaxTextureNameBytes - 1, _asset, this);

	_ui.TextureName->setValidator(textureNameValidator);

	connect(_ui.Textures, qOverload<int>(&QComboBox::currentIndexChanged), textureNameValidator, &UniqueTextureNameValidator::SetCurrentIndex);

	connect(_asset, &StudioModelAsset::ModelChanged, this, &StudioModelTexturesPanel::OnModelChanged);
	connect(_asset, &StudioModelAsset::SaveSnapshot, this, &StudioModelTexturesPanel::OnSaveSnapshot);
	connect(_asset, &StudioModelAsset::LoadSnapshot, this, &StudioModelTexturesPanel::OnLoadSnapshot);

	connect(_ui.Textures, qOverload<int>(&QComboBox::currentIndexChanged), this, &StudioModelTexturesPanel::OnTextureChanged);
	connect(_ui.ScaleTextureViewSlider, &QSlider::valueChanged, this, &StudioModelTexturesPanel::OnTextureViewScaleSliderChanged);
	connect(_ui.ScaleTextureViewSpinner, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &StudioModelTexturesPanel::OnTextureViewScaleSpinnerChanged);
	connect(_ui.UVLineWidthSlider, &QSlider::valueChanged, this, &StudioModelTexturesPanel::OnUVLineWidthSliderChanged);
	connect(_ui.UVLineWidthSpinner, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &StudioModelTexturesPanel::OnUVLineWidthSpinnerChanged);

	connect(_ui.TextureName, &QLineEdit::textChanged, this, &StudioModelTexturesPanel::OnTextureNameChanged);
	connect(_ui.TextureName, &QLineEdit::inputRejected, this, &StudioModelTexturesPanel::OnTextureNameRejected);

	connect(_ui.Chrome, &QCheckBox::stateChanged, this, &StudioModelTexturesPanel::OnChromeChanged);
	connect(_ui.Additive, &QCheckBox::stateChanged, this, &StudioModelTexturesPanel::OnAdditiveChanged);
	connect(_ui.Transparent, &QCheckBox::stateChanged, this, &StudioModelTexturesPanel::OnTransparentChanged);
	connect(_ui.FlatShade, &QCheckBox::stateChanged, this, &StudioModelTexturesPanel::OnFlatShadeChanged);
	connect(_ui.Fullbright, &QCheckBox::stateChanged, this, &StudioModelTexturesPanel::OnFullbrightChanged);
	connect(_ui.Mipmaps, &QCheckBox::stateChanged, this, &StudioModelTexturesPanel::OnMipmapsChanged);

	connect(_ui.ShowUVMap, &QCheckBox::stateChanged, this, &StudioModelTexturesPanel::OnShowUVMapChanged);
	connect(_ui.OverlayUVMap, &QCheckBox::stateChanged, this, &StudioModelTexturesPanel::OnOverlayUVMapChanged);
	connect(_ui.AntiAliasLines, &QCheckBox::stateChanged, this, &StudioModelTexturesPanel::OnAntiAliasLinesChanged);

	connect(_ui.Meshes, qOverload<int>(&QComboBox::currentIndexChanged), this, &StudioModelTexturesPanel::OnMeshChanged);

	connect(_ui.ImportTexture, &QPushButton::clicked, this, &StudioModelTexturesPanel::OnImportTexture);
	connect(_ui.ExportTexture, &QPushButton::clicked, this, &StudioModelTexturesPanel::OnExportTexture);
	connect(_ui.ExportUVMap, &QPushButton::clicked, this, &StudioModelTexturesPanel::OnExportUVMap);

	connect(_ui.ImportAllTextures, &QPushButton::clicked, this, &StudioModelTexturesPanel::OnImportAllTextures);
	connect(_ui.ExportAllTextures, &QPushButton::clicked, this, &StudioModelTexturesPanel::OnExportAllTextures);

	connect(_ui.TopColorSlider, &QSlider::valueChanged, this, &StudioModelTexturesPanel::OnTopColorSliderChanged);
	connect(_ui.BottomColorSlider, &QSlider::valueChanged, this, &StudioModelTexturesPanel::OnBottomColorSliderChanged);
	connect(_ui.TopColorSpinner, qOverload<int>(&QSpinBox::valueChanged), this, &StudioModelTexturesPanel::OnTopColorSpinnerChanged);
	connect(_ui.BottomColorSpinner, qOverload<int>(&QSpinBox::valueChanged), this, &StudioModelTexturesPanel::OnBottomColorSpinnerChanged);

	connect(_ui.MinFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, &StudioModelTexturesPanel::OnTextureFiltersChanged);
	connect(_ui.MagFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, &StudioModelTexturesPanel::OnTextureFiltersChanged);
	connect(_ui.MipmapFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, &StudioModelTexturesPanel::OnTextureFiltersChanged);

	connect(_ui.PowerOf2Textures, &QCheckBox::stateChanged, this, &StudioModelTexturesPanel::OnPowerOf2TexturesChanged);

	const auto studioModelSettings{_asset->GetProvider()->GetStudioModelSettings()};

	_ui.MinFilter->setCurrentIndex(static_cast<int>(studioModelSettings->GetMinFilter()));
	_ui.MagFilter->setCurrentIndex(static_cast<int>(studioModelSettings->GetMagFilter()));
	_ui.MipmapFilter->setCurrentIndex(static_cast<int>(studioModelSettings->GetMipmapFilter()));

	//Force an update to ensure the loader is correctly configured
	//TODO: maybe do this somewhere else?
	OnTextureFiltersChanged();

	_ui.PowerOf2Textures->setChecked(studioModelSettings->ShouldResizeTexturesToPowerOf2());

	_ui.ScaleTextureViewSpinner->setRange(TextureViewScaleMinimum, TextureViewScaleMaximum);
	_ui.ScaleTextureViewSpinner->setValue(TextureViewScaleDefault);
	_ui.ScaleTextureViewSpinner->setSingleStep(TextureViewScaleSingleStepValue);

	_ui.ScaleTextureViewSlider->setRange(
		0,
		static_cast<int>((_ui.ScaleTextureViewSpinner->maximum() - _ui.ScaleTextureViewSpinner->minimum()) * TextureViewScaleSliderRatio));

	_ui.ScaleTextureViewSlider->setValue(
		static_cast<int>((_ui.ScaleTextureViewSpinner->value() - _ui.ScaleTextureViewSpinner->minimum()) * TextureViewScaleSliderRatio));

	_ui.UVLineWidthSlider->setRange(
		static_cast<int>(_ui.UVLineWidthSpinner->minimum() * UVLineWidthSliderRatio),
		static_cast<int>(_ui.UVLineWidthSpinner->maximum() * UVLineWidthSliderRatio));

	_ui.UVLineWidthSlider->setValue(static_cast<int>(_ui.UVLineWidthSpinner->value() * UVLineWidthSliderRatio));

	InitializeUI();
}

StudioModelTexturesPanel::~StudioModelTexturesPanel() = default;

void StudioModelTexturesPanel::OnMouseEvent(QMouseEvent* event)
{
	switch (event->type())
	{
	case QEvent::Type::MouseButtonPress:
	{
		//Only reset the position if a single button is down
		if (event->buttons() & (Qt::MouseButton::LeftButton | Qt::MouseButton::RightButton) && !(event->buttons() & (event->buttons() << 1)))
		{
			const auto position = event->pos();

			_dragPosition.x = position.x();
			_dragPosition.y = position.y();

			_trackedMouseButtons.setFlag(event->button(), true);
		}
		break;
	}

	case QEvent::Type::MouseButtonRelease:
	{
		_trackedMouseButtons.setFlag(event->button(), false);
		break;
	}

	case QEvent::Type::MouseMove:
	{
		const glm::ivec2 position{event->pos().x(), event->pos().y()};

		const glm::ivec2 delta = position - _dragPosition;

		if (_trackedMouseButtons & Qt::MouseButton::LeftButton && event->buttons() & Qt::MouseButton::LeftButton)
		{
			auto scene = _asset->GetScene();

			scene->TextureXOffset += delta.x;
			scene->TextureYOffset += delta.y;
		}
		else if (_trackedMouseButtons & Qt::MouseButton::RightButton && event->buttons() & Qt::MouseButton::RightButton)
		{
			const double zoomAdjust = delta.y / -20.0;

			_ui.ScaleTextureViewSpinner->setValue(_ui.ScaleTextureViewSpinner->value() + zoomAdjust);
		}

		_dragPosition = position;
		break;
	}
	}
}

QImage StudioModelTexturesPanel::CreateUVMapImage(
	StudioModelEntity* entity, const int textureIndex, const int meshIndex, const bool antiAliasLines, float textureScale, qreal lineWidth)
{
	const auto model = entity->GetEditableModel();

	const auto& texture = *model->Textures[textureIndex];

	//RGBA format because only the UV lines need to be drawn, with no background
	QImage image{static_cast<int>(std::ceil(texture.Width * textureScale)), static_cast<int>(std::ceil(texture.Height * textureScale)),
		QImage::Format::Format_RGBA8888};

	//Set as transparent
	image.fill(Qt::transparent);

	QPainter painter{&image};

	painter.setPen(QPen{Qt::white, lineWidth});
	painter.setRenderHint(QPainter::RenderHint::Antialiasing, antiAliasLines);

	auto fixCoords = [=](int x, int y)
	{
		return QPointF(x * textureScale, y * textureScale);
	};

	auto meshes = entity->ComputeMeshList(textureIndex);

	if (meshIndex != -1)
	{
		auto singleMesh = meshes[meshIndex];
		meshes.clear();
		meshes.emplace_back(singleMesh);
	}

	for (const auto mesh : meshes)
	{
		auto ptricmds = mesh->Triangles.data();

		for (int i; i = *(ptricmds++);)
		{
			if (i < 0)
			{
				i = -i;

				const auto firstVertex{fixCoords(ptricmds[2], ptricmds[3])};

				ptricmds += 4;
				--i;

				for (; i > 0; --i, ptricmds += 4)
				{
					painter.drawLine(firstVertex, fixCoords(ptricmds[2], ptricmds[3]));

					if (i > 1)
					{
						painter.drawLine(fixCoords(ptricmds[2], ptricmds[3]), fixCoords(ptricmds[6], ptricmds[7]));
					}
				}
			}
			else
			{
				auto firstVertex{fixCoords(ptricmds[2], ptricmds[3])};
				auto secondVertex{fixCoords(ptricmds[6], ptricmds[7])};

				painter.drawLine(firstVertex, secondVertex);

				ptricmds += 8;
				i -= 2;

				for (; i > 0; --i, ptricmds += 4)
				{
					painter.drawLine(secondVertex, fixCoords(ptricmds[2], ptricmds[3]));
					painter.drawLine(fixCoords(ptricmds[2], ptricmds[3]), firstVertex);

					firstVertex = secondVertex;
					secondVertex = fixCoords(ptricmds[2], ptricmds[3]);
				}
			}
		}
	}

	return image;
}

void StudioModelTexturesPanel::DrawUVImage(const QColor& backgroundColor, bool overlayOnTexture, const QImage& texture, const QImage& uvMap, QImage& target)
{
	target.fill(backgroundColor);

	QPainter painter{&target};

	const QRect drawRect{0, 0, target.width(), target.height()};

	if (overlayOnTexture)
	{
		painter.drawImage(drawRect, texture);
	}

	painter.drawImage(drawRect, uvMap);
}

void StudioModelTexturesPanel::InitializeUI()
{
	auto model = _asset->GetScene()->GetEntity()->GetEditableModel();

	this->setEnabled(!model->Textures.empty());

	_ui.Textures->clear();

	QStringList textures;

	textures.reserve(model->Textures.size());

	for (std::size_t i = 0; i < model->Textures.size(); ++i)
	{
		textures.append(FormatTextureName(*model->Textures[i]));
	}

	_ui.Textures->addItems(textures);
}

void StudioModelTexturesPanel::OnCreateDeviceResources()
{
	//TODO: this shouldn't be done here
	RemapTextures();
}

void StudioModelTexturesPanel::OnDockPanelChanged(QWidget* current, QWidget* previous)
{
	const bool wasActive = _asset->GetScene()->ShowTexture;

	_asset->GetScene()->ShowTexture = this == current;

	if (_asset->GetScene()->ShowTexture && !wasActive)
	{
		_asset->PushInputSink(this);
	}
	else if (!_asset->GetScene()->ShowTexture && wasActive)
	{
		_asset->PopInputSink();
	}
}

static void SetTextureFlagCheckBoxes(Ui_StudioModelTexturesPanel& ui, int flags)
{
	const QSignalBlocker chrome{ui.Chrome};
	const QSignalBlocker additive{ui.Additive};
	const QSignalBlocker transparent{ui.Transparent};
	const QSignalBlocker flatShade{ui.FlatShade};
	const QSignalBlocker fullbright{ui.Fullbright};
	const QSignalBlocker nomipmaps{ui.Mipmaps};

	ui.Chrome->setChecked((flags & STUDIO_NF_CHROME) != 0);
	ui.Additive->setChecked((flags & STUDIO_NF_ADDITIVE) != 0);
	ui.Transparent->setChecked((flags & STUDIO_NF_MASKED) != 0);
	ui.FlatShade->setChecked((flags & STUDIO_NF_FLATSHADE) != 0);
	ui.Fullbright->setChecked((flags & STUDIO_NF_FULLBRIGHT) != 0);

	//TODO: change the constant name to reflect the actual behavior (flag enables mipmaps)
	ui.Mipmaps->setChecked((flags & STUDIO_NF_NOMIPS) != 0);
}

void StudioModelTexturesPanel::OnModelChanged(const ModelChangeEvent& event)
{
	const auto model = _asset->GetScene()->GetEntity()->GetEditableModel();

	switch (event.GetId())
	{
	case ModelChangeId::ImportTexture:
		//Use the same code for texture name changes
		[[fallthrough]];

	case ModelChangeId::ChangeTextureName:
	{
		const auto& listChange{static_cast<const ModelListChangeEvent&>(event)};

		const auto& texture = *model->Textures[listChange.GetSourceIndex()];

		_ui.Textures->setItemText(listChange.GetSourceIndex(), FormatTextureName(texture));

		if (listChange.GetSourceIndex() == _ui.Textures->currentIndex())
		{
			const QString name{texture.Name.c_str()};

			//Avoid resetting the edit position
			if (_ui.TextureName->text() != name)
			{
				const QSignalBlocker blocker{_ui.TextureName};
				_ui.TextureName->setText(name);
			}
		}

		if (event.GetId() == ModelChangeId::ChangeTextureName)
		{
			//TODO: shouldn't be done here
			RemapTexture(listChange.GetSourceIndex());
		}
		break;
	}

	case ModelChangeId::ChangeTextureFlags:
	{
		const auto& listChange{static_cast<const ModelListChangeEvent&>(event)};

		const auto& texture = model->Textures[listChange.GetSourceIndex()];

		//TODO: shouldn't be done here
		auto graphicsContext = _asset->GetScene()->GetGraphicsContext();

		graphicsContext->Begin();
		model->ReuploadTexture(*_asset->GetTextureLoader(), texture.get());
		RemapTexture(listChange.GetSourceIndex());
		graphicsContext->End();

		if (listChange.GetSourceIndex() == _ui.Textures->currentIndex())
		{
			SetTextureFlagCheckBoxes(_ui, texture->Flags);
		}
		break;
	}
	}
}

void StudioModelTexturesPanel::OnSaveSnapshot(StateSnapshot* snapshot)
{
	if (auto index = _ui.Textures->currentIndex(); index != -1)
	{
		auto model = _asset->GetScene()->GetEntity()->GetEditableModel();

		const auto& texture = *model->Textures[index];

		snapshot->SetValue("textures.texture", QString::fromStdString(texture.Name));
	}
}

void StudioModelTexturesPanel::OnLoadSnapshot(StateSnapshot* snapshot)
{
	InitializeUI();

	if (auto texture = snapshot->Value("textures.texture"); texture.isValid())
	{
		auto textureName = texture.toString().toStdString();

		auto model = _asset->GetScene()->GetEntity()->GetEditableModel();

		if (auto it = std::find_if(model->Textures.begin(), model->Textures.end(), [&](const auto& texture)
			{
				return texture->Name == textureName;
			}); it != model->Textures.end())
		{
			const auto index = it - model->Textures.begin();

			_ui.Textures->setCurrentIndex(index);
		}
	}
}

void StudioModelTexturesPanel::OnTextureChanged(int index)
{
	auto scene = _asset->GetScene();

	//Reset texture position to be centered
	scene->TextureXOffset = scene->TextureYOffset = 0;
	scene->TextureIndex = index;

	_ui.Meshes->clear();
	_ui.Meshes->setEnabled(index != -1);

	const studiomdl::Texture emptyTexture{};

	auto entity = scene->GetEntity();

	const auto& texture = index != -1 ? *entity->GetEditableModel()->Textures[index] : emptyTexture;

	{
		const QSignalBlocker name{_ui.TextureName};

		_ui.TextureName->setText(texture.Name.c_str());
	}

	SetTextureFlagCheckBoxes(_ui, texture.Flags);

	const auto meshes = entity->ComputeMeshList(index);

	for (decltype(meshes.size()) i = 0; i < meshes.size(); ++i)
	{
		_ui.Meshes->addItem(QString{"Mesh %1"}.arg(i + 1));
	}

	if (_ui.Meshes->count() > 1)
	{
		_ui.Meshes->addItem("All");
	}

	UpdateUVMapTexture();
}

void StudioModelTexturesPanel::OnTextureViewScaleSliderChanged(int value)
{
	const double newValue = (value / UVLineWidthSliderRatio) + _ui.ScaleTextureViewSpinner->minimum();

	{
		const QSignalBlocker blocker{_ui.ScaleTextureViewSpinner};
		_ui.ScaleTextureViewSpinner->setValue(newValue);
	}

	_asset->GetScene()->TextureScale = newValue;

	UpdateUVMapTexture();
}

void StudioModelTexturesPanel::OnTextureViewScaleSpinnerChanged(double value)
{
	{
		const QSignalBlocker blocker{_ui.ScaleTextureViewSlider};
		_ui.ScaleTextureViewSlider->setValue(static_cast<int>((value - _ui.ScaleTextureViewSpinner->minimum()) * UVLineWidthSliderRatio));
	}

	_asset->GetScene()->TextureScale = value;

	UpdateUVMapTexture();
}

void StudioModelTexturesPanel::OnUVLineWidthSliderChanged(int value)
{
	const double newValue = value / UVLineWidthSliderRatio;

	_uvLineWidth = static_cast<qreal>(newValue);

	{
		const QSignalBlocker blocker{_ui.UVLineWidthSpinner};
		_ui.UVLineWidthSpinner->setValue(newValue);
	}

	UpdateUVMapTexture();
}

void StudioModelTexturesPanel::OnUVLineWidthSpinnerChanged(double value)
{
	_uvLineWidth = static_cast<qreal>(value);

	{
		const QSignalBlocker blocker{_ui.UVLineWidthSlider};
		_ui.UVLineWidthSlider->setValue(static_cast<int>(value * UVLineWidthSliderRatio));
	}

	UpdateUVMapTexture();
}

void StudioModelTexturesPanel::OnTextureNameChanged()
{
	const auto& texture = *_asset->GetScene()->GetEntity()->GetEditableModel()->Textures[_ui.Textures->currentIndex()];

	_asset->AddUndoCommand(new ChangeTextureNameCommand(_asset, _ui.Textures->currentIndex(), texture.Name.c_str(), _ui.TextureName->text()));
}

void StudioModelTexturesPanel::OnTextureNameRejected()
{
	QToolTip::showText(_ui.TextureName->mapToGlobal({0, -20}), "Texture names must be unique");
}

void StudioModelTexturesPanel::OnChromeChanged()
{
	const auto& texture = *_asset->GetScene()->GetEntity()->GetEditableModel()->Textures[_ui.Textures->currentIndex()];

	int flags = texture.Flags;

	//Chrome disables alpha testing
	if (_ui.Chrome->isChecked())
	{
		flags = SetFlags(flags, STUDIO_NF_MASKED, false);
	}

	flags = SetFlags(flags, STUDIO_NF_CHROME, _ui.Chrome->isChecked());

	_asset->AddUndoCommand(new ChangeTextureFlagsCommand(_asset, _ui.Textures->currentIndex(), texture.Flags, flags));
}

void StudioModelTexturesPanel::OnAdditiveChanged()
{
	const auto& texture = *_asset->GetScene()->GetEntity()->GetEditableModel()->Textures[_ui.Textures->currentIndex()];

	int flags = texture.Flags;

	//Additive disables alpha testing
	if (_ui.Additive->isChecked())
	{
		flags = SetFlags(flags, STUDIO_NF_MASKED, false);
	}

	flags = SetFlags(flags, STUDIO_NF_ADDITIVE, _ui.Additive->isChecked());

	_asset->AddUndoCommand(new ChangeTextureFlagsCommand(_asset, _ui.Textures->currentIndex(), texture.Flags, flags));
}

void StudioModelTexturesPanel::OnTransparentChanged()
{
	const auto& texture = *_asset->GetScene()->GetEntity()->GetEditableModel()->Textures[_ui.Textures->currentIndex()];

	int flags = texture.Flags;

	//Alpha testing disables chrome and additive
	if (_ui.Transparent->isChecked())
	{
		flags = SetFlags(flags, STUDIO_NF_CHROME | STUDIO_NF_ADDITIVE, false);
	}

	flags = SetFlags(flags, STUDIO_NF_MASKED, _ui.Transparent->isChecked());

	_asset->AddUndoCommand(new ChangeTextureFlagsCommand(_asset, _ui.Textures->currentIndex(), texture.Flags, flags));
}

void StudioModelTexturesPanel::OnFlatShadeChanged()
{
	const auto& texture = *_asset->GetScene()->GetEntity()->GetEditableModel()->Textures[_ui.Textures->currentIndex()];

	int flags = texture.Flags;

	flags = SetFlags(flags, STUDIO_NF_FLATSHADE, _ui.FlatShade->isChecked());

	_asset->AddUndoCommand(new ChangeTextureFlagsCommand(_asset, _ui.Textures->currentIndex(), texture.Flags, flags));
}

void StudioModelTexturesPanel::OnFullbrightChanged()
{
	const auto& texture = *_asset->GetScene()->GetEntity()->GetEditableModel()->Textures[_ui.Textures->currentIndex()];

	int flags = texture.Flags;

	flags = SetFlags(flags, STUDIO_NF_FULLBRIGHT, _ui.Fullbright->isChecked());

	_asset->AddUndoCommand(new ChangeTextureFlagsCommand(_asset, _ui.Textures->currentIndex(), texture.Flags, flags));
}

void StudioModelTexturesPanel::OnMipmapsChanged()
{
	const auto& texture = *_asset->GetScene()->GetEntity()->GetEditableModel()->Textures[_ui.Textures->currentIndex()];

	int flags = texture.Flags;

	flags = SetFlags(flags, STUDIO_NF_NOMIPS, _ui.Mipmaps->isChecked());

	_asset->AddUndoCommand(new ChangeTextureFlagsCommand(_asset, _ui.Textures->currentIndex(), texture.Flags, flags));
}

void StudioModelTexturesPanel::OnShowUVMapChanged()
{
	UpdateUVMapTexture();
}

void StudioModelTexturesPanel::OnOverlayUVMapChanged()
{
	_asset->GetScene()->OverlayUVMap = _ui.OverlayUVMap->isChecked();
}

void StudioModelTexturesPanel::OnAntiAliasLinesChanged()
{
	UpdateUVMapTexture();
}

void StudioModelTexturesPanel::OnMeshChanged(int index)
{
	UpdateUVMapTexture();
}

void StudioModelTexturesPanel::ImportTextureFrom(const QString& fileName, studiomdl::EditableStudioModel& model, int textureIndex)
{
	QImage image{fileName};

	if (image.isNull())
	{
		QMessageBox::critical(this, "Error loading image", QString{"Failed to load image \"%1\"."}.arg(fileName));
		return;
	}

	const QImage::Format inputFormat = image.format();

	if (inputFormat != QImage::Format::Format_Indexed8)
	{
		QMessageBox::warning(this, "Warning",
			QString{"Image \"%1\" has the format \"%2\" and will be converted to an indexed 8 bit image. Loss of color depth may occur."}
				.arg(fileName)
				.arg(QMetaEnum::fromType<QImage::Format>().valueToKey(inputFormat)));
		
		image.convertTo(QImage::Format::Format_Indexed8);
	}

	const QVector<QRgb> palette = image.colorTable();

	if (palette.isEmpty())
	{
		QMessageBox::critical(this, "Error loading image", QString{"Palette for image \"%1\" does not exist."}.arg(fileName));
		return;
	}

	auto& texture = *model.Textures[textureIndex];

	//Convert to 8 bit palette based image
	std::unique_ptr<byte[]> texData = std::make_unique<byte[]>(image.width() * image.height());

	{
		byte* pDest = texData.get();

		for (int y = 0; y < image.height(); ++y)
		{
			for (int x = 0; x < image.width(); ++x, ++pDest)
			{
				*pDest = image.pixelIndex(x, y);
			}
		}
	}

	graphics::RGBPalette convPal;

	int paletteIndex;

	for (paletteIndex = 0; paletteIndex < palette.size(); ++paletteIndex)
	{
		const auto rgb = palette[paletteIndex];

		convPal[paletteIndex] =
		{
			static_cast<byte>(qRed(rgb)),
			static_cast<byte>(qGreen(rgb)),
			static_cast<byte>(qBlue(rgb))
		};
	}

	//Fill remaining entries with black
	for (; paletteIndex < convPal.EntriesCount; ++paletteIndex)
	{
		convPal[paletteIndex] = {0, 0, 0};
	}

	auto scaledSTCoordinates = studiomdl::CalculateScaledSTCoordinatesData(
		model, textureIndex, texture.Width, texture.Height, image.width(), image.height());

	ImportTextureData oldTexture;
	ImportTextureData newTexture;

	oldTexture.Width = texture.Width;
	oldTexture.Height = texture.Height;
	oldTexture.Pixels = std::make_unique<byte[]>(oldTexture.Width * oldTexture.Height);

	memcpy(oldTexture.Pixels.get(), texture.Pixels.data(), texture.Pixels.size());
	oldTexture.Palette = texture.Palette;
	oldTexture.ScaledSTCoordinates = std::move(scaledSTCoordinates.first);

	newTexture.Width = image.width();
	newTexture.Height = image.height();
	newTexture.Pixels = std::move(texData);
	newTexture.Palette = convPal;
	newTexture.ScaledSTCoordinates = std::move(scaledSTCoordinates.second);

	_asset->AddUndoCommand(new ImportTextureCommand(_asset, textureIndex, std::move(oldTexture), std::move(newTexture)));
}

static QImage ConvertTextureToRGBImage(
	const studiomdl::Texture& texture, const byte* textureData, const graphics::RGBPalette& texturePalette, std::vector<QRgb>& dataBuffer)
{
	dataBuffer.resize(texture.Width * texture.Height);

	for (int y = 0; y < texture.Height; ++y)
	{
		for (int x = 0; x < texture.Width; ++x)
		{
			const auto& color = texturePalette[textureData[(texture.Width * y) + x]];

			dataBuffer[(texture.Width * y) + x] = qRgb(color.R, color.G, color.B);
		}
	}

	return QImage{reinterpret_cast<const uchar*>(dataBuffer.data()), texture.Width, texture.Height, QImage::Format::Format_RGB32};
}

bool StudioModelTexturesPanel::ExportTextureTo(const QString& fileName, const studiomdl::EditableStudioModel& model, const studiomdl::Texture& texture)
{
	//Ensure data is 32 bit aligned
	const int alignedWidth = (texture.Width + 3) & (~3);

	std::vector<uchar> alignedPixels;

	alignedPixels.resize(alignedWidth * texture.Height);

	for (int h = 0; h < texture.Height; ++h)
	{
		for (int w = 0; w < texture.Width; ++w)
		{
			alignedPixels[(alignedWidth * h) + w] = texture.Pixels[(texture.Width * h) + w];
		}
	}

	QImage textureImage{alignedPixels.data(), texture.Width, texture.Height, QImage::Format::Format_Indexed8};

	QVector<QRgb> palette;

	palette.reserve(texture.Palette.GetSizeInBytes());

	for (const auto& rgb : texture.Palette)
	{
		palette.append(qRgb(rgb.R, rgb.G, rgb.B));
	}

	textureImage.setColorTable(palette);

	if (!textureImage.save(fileName))
	{
		return false;
	}

	return true;
}

void StudioModelTexturesPanel::RemapTexture(int index)
{
	auto model = _asset->GetScene()->GetEntity()->GetEditableModel();

	auto& texture = *model->Textures[index];

	int low, mid, high;

	if (graphics::TryGetRemapColors(texture.Name.c_str(), low, mid, high))
	{
		graphics::RGBPalette palette{texture.Palette};

		graphics::PaletteHueReplace(palette, _ui.TopColorSlider->value(), low, mid);

		if (high)
		{
			graphics::PaletteHueReplace(palette, _ui.BottomColorSlider->value(), mid + 1, high);
		}

		auto graphicsContext = _asset->GetScene()->GetGraphicsContext();

		graphicsContext->Begin();
		model->ReplaceTexture(*_asset->GetTextureLoader(), &texture, texture.Pixels.data(), palette);
		graphicsContext->End();
	}
}

void StudioModelTexturesPanel::RemapTextures()
{
	auto model = _asset->GetScene()->GetEntity()->GetEditableModel();

	for (int i = 0; i < model->Textures.size(); ++i)
	{
		RemapTexture(i);
	}
}

void StudioModelTexturesPanel::UpdateColormapValue()
{
	const auto topColor = _ui.TopColorSlider->value();
	const auto bottomColor = _ui.BottomColorSlider->value();

	const int colormapValue = topColor & 0xFF | ((bottomColor & 0xFF) << 8);

	_ui.ColormapValue->setText(QString::number(colormapValue));
}

void StudioModelTexturesPanel::UpdateUVMapTexture()
{
	auto scene = _asset->GetScene();

	scene->ShowUVMap = _ui.ShowUVMap->isChecked();

	if (!_ui.ShowUVMap->isChecked())
	{
		return;
	}

	const int textureIndex = _ui.Textures->currentIndex();

	if (textureIndex == -1)
	{
		//Make sure nothing is drawn when no texture is selected
		const byte transparentImage[4] = {0, 0, 0, 0};

		glBindTexture(GL_TEXTURE_2D, scene->UVMeshTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
			1, 1,
			0, GL_RGBA, GL_UNSIGNED_BYTE, transparentImage);
		return;
	}

	auto entity = scene->GetEntity();

	auto model = entity->GetEditableModel();

	const auto& texture = *model->Textures[textureIndex];

	scene->ShowUVMap = _ui.ShowUVMap->isChecked();

	const float scale = scene->TextureScale;

	//Create an updated image of the UV map with current settings
	const auto uvMapImage = CreateUVMapImage(entity, textureIndex, GetMeshIndexForDrawing(_ui.Meshes), _ui.AntiAliasLines->isChecked(), scale, _uvLineWidth);

	scene->GetGraphicsContext()->Begin();

	glBindTexture(GL_TEXTURE_2D, scene->UVMeshTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		static_cast<int>(std::ceil(texture.Width * scale)), static_cast<int>(std::ceil(texture.Height * scale)),
		0, GL_RGBA, GL_UNSIGNED_BYTE, uvMapImage.constBits());

	//Nearest filtering causes gaps in lines, linear does not
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	//Prevent the texture from wrapping and spilling over on the other side
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	scene->GetGraphicsContext()->End();
}

void StudioModelTexturesPanel::OnImportTexture()
{
	auto model = _asset->GetScene()->GetEntity()->GetEditableModel();

	const int iTextureIndex = _ui.Textures->currentIndex();

	if (iTextureIndex == -1)
	{
		QMessageBox::information(this, "Message", "No texture selected");
		return;
	}

	const QString fileName = QFileDialog::getOpenFileName(this, {}, {}, qt::GetImagesFileFilter());

	if (fileName.isEmpty())
	{
		return;
	}

	ImportTextureFrom(fileName, *model, iTextureIndex);

	RemapTexture(iTextureIndex);
}

void StudioModelTexturesPanel::OnExportTexture()
{
	const int textureIndex = _ui.Textures->currentIndex();

	if (textureIndex == -1)
	{
		QMessageBox::information(this, "Message", "No texture selected");
		return;
	}

	auto model = _asset->GetScene()->GetEntity()->GetEditableModel();

	const auto& texture = *model->Textures[textureIndex];

	const QString fileName = QFileDialog::getSaveFileName(this, {}, texture.Name.c_str(), qt::GetSeparatedImagesFileFilter());

	if (fileName.isEmpty())
	{
		return;
	}

	if (!ExportTextureTo(fileName, *model, texture))
	{
		QMessageBox::critical(this, "Error", QString{"Failed to save image \"%1\""}.arg(fileName));
	}
}

void StudioModelTexturesPanel::OnExportUVMap()
{
	const int textureIndex = _ui.Textures->currentIndex();

	if (textureIndex == -1)
	{
		QMessageBox::information(this, "Message", "No texture selected");
		return;
	}

	auto entity = _asset->GetScene()->GetEntity();

	const auto model = entity->GetEditableModel();

	const auto& texture = *model->Textures[textureIndex];

	const auto textureData = texture.Pixels.data();

	std::vector<QRgb> dataBuffer;

	auto textureImage{ConvertTextureToRGBImage(texture, textureData, texture.Palette, dataBuffer)};

	if (StudioModelExportUVMeshDialog dialog{entity, textureIndex, GetMeshIndexForDrawing(_ui.Meshes), textureImage, this};
		QDialog::DialogCode::Accepted == dialog.exec())
	{
		//Redraw the final image with a transparent background
		const auto uvMapImage = dialog.GetUVImage();

		QImage resultImage{uvMapImage.width(), uvMapImage.height(), QImage::Format::Format_RGBA8888};

		//Set as transparent
		StudioModelTexturesPanel::DrawUVImage(Qt::transparent, dialog.ShouldOverlayOnTexture(), textureImage, dialog.GetUVImage(), resultImage);

		if (!dialog.ShouldAddAlphaChannel())
		{
			resultImage.convertTo(QImage::Format::Format_RGB888);
		}

		const QString fileName{dialog.GetFileName()};

		if (!resultImage.save(fileName))
		{
			QMessageBox::critical(this, "Error", QString{"Failed to save image \"%1\""}.arg(fileName));
		}
	}
}

void StudioModelTexturesPanel::OnImportAllTextures()
{
	const auto path = QFileDialog::getExistingDirectory(this, "Select the directory to import all textures from");

	if (path.isEmpty())
	{
		return;
	}

	auto entity = _asset->GetScene()->GetEntity();

	auto model = entity->GetEditableModel();

	_asset->GetUndoStack()->beginMacro("Import all textures");

	//For each texture in the model, find if there is a file with the same name in the given directory
	//If so, try to replace the texture
	for (int i = 0; i < model->Textures.size(); ++i)
	{
		auto& texture = *model->Textures[i];

		const QFileInfo fileName{path, texture.Name.c_str()};

		if (fileName.exists())
		{
			ImportTextureFrom(fileName.absoluteFilePath(), *model, i);
		}
	}

	_asset->GetUndoStack()->endMacro();

	RemapTextures();
}

void StudioModelTexturesPanel::OnExportAllTextures()
{
	const auto path = QFileDialog::getExistingDirectory(this, "Select the directory to export all textures to");

	if (path.isEmpty())
	{
		return;
	}

	auto model = _asset->GetScene()->GetEntity()->GetEditableModel();

	QString errors;

	for (int i = 0; i < model->Textures.size(); ++i)
	{
		const auto& texture = *model->Textures[i];

		const QFileInfo fileName{path, texture.Name.c_str()};

		auto fullPath = fileName.absoluteFilePath();
		
		if (!ExportTextureTo(fullPath, *model, texture))
		{
			errors += QString{"\"%1\"\n"}.arg(fullPath);
		}
	}

	if (!errors.isEmpty())
	{
		QMessageBox::warning(this, "One or more errors occurred", QString{"Failed to save images:\n%1"}.arg( errors));
	}
}

void StudioModelTexturesPanel::OnTopColorSliderChanged()
{
	_ui.TopColorSpinner->setValue(_ui.TopColorSlider->value());

	UpdateColormapValue();
	RemapTextures();
}

void StudioModelTexturesPanel::OnBottomColorSliderChanged()
{
	_ui.BottomColorSpinner->setValue(_ui.BottomColorSlider->value());

	UpdateColormapValue();
	RemapTextures();
}

void StudioModelTexturesPanel::OnTopColorSpinnerChanged()
{
	_ui.TopColorSlider->setValue(_ui.TopColorSpinner->value());

	UpdateColormapValue();
	RemapTextures();
}

void StudioModelTexturesPanel::OnBottomColorSpinnerChanged()
{
	_ui.BottomColorSlider->setValue(_ui.BottomColorSpinner->value());

	UpdateColormapValue();
	RemapTextures();
}

void StudioModelTexturesPanel::OnTextureFiltersChanged()
{
	const auto textureLoader{_asset->GetTextureLoader()};

	textureLoader->SetTextureFilters(
		static_cast<graphics::TextureFilter>(_ui.MinFilter->currentIndex()),
		static_cast<graphics::TextureFilter>(_ui.MagFilter->currentIndex()),
		static_cast<graphics::MipmapFilter>(_ui.MipmapFilter->currentIndex()));

	const auto graphicsContext = _asset->GetScene()->GetGraphicsContext();

	graphicsContext->Begin();
	_asset->GetScene()->GetEntity()->GetEditableModel()->UpdateFilters(*textureLoader);
	graphicsContext->End();
}

void StudioModelTexturesPanel::OnPowerOf2TexturesChanged()
{
	_asset->GetTextureLoader()->SetResizeToPowerOf2(_ui.PowerOf2Textures->isChecked());

	const auto graphicsContext = _asset->GetScene()->GetGraphicsContext();

	graphicsContext->Begin();
	_asset->GetScene()->GetEntity()->GetEditableModel()->ReuploadTextures(*_asset->GetTextureLoader());
	graphicsContext->End();
}
}
