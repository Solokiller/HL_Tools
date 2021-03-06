#include <algorithm>
#include <stdexcept>

#include "assets/AssetIO.hpp"
#include "ui/assets/Assets.hpp"
#include "utility/IOUtils.hpp"

namespace ui::assets
{
std::vector<AssetProvider*> AssetProviderRegistry::GetAssetProviders() const
{
	std::vector<AssetProvider*> providers;

	providers.reserve(_providers.size());

	std::transform(_providers.begin(), _providers.end(), std::back_inserter(providers), [](const auto& provider)
		{
			return provider.get();
		});

	return providers;
}

void AssetProviderRegistry::AddProvider(std::unique_ptr<AssetProvider>&& provider)
{
	if (std::find_if(_providers.begin(), _providers.end(), [&](const auto& other)
		{
			return other->GetProviderName() == provider->GetProviderName();
		}) != _providers.end())
	{
		throw std::invalid_argument("Only one provider can be registered for a particular asset type");
	}

	_providers.push_back(std::move(provider));
}

std::unique_ptr<Asset> AssetProviderRegistry::Load(EditorContext* editorContext, const QString& fileName) const
{
	std::unique_ptr<FILE, decltype(::fclose)*> file{utf8_exclusive_read_fopen(fileName.toStdString().c_str(), true), &::fclose};

	if (!file)
	{
		throw ::assets::AssetException("Could not open asset: file does not exist or is currently opened by another program");
	}

	for (const auto& provider : _providers)
	{
		if (provider->CanLoad(fileName, file.get()))
		{
			rewind(file.get());
			return provider->Load(editorContext, fileName, file.get());
		}

		rewind(file.get());
	}

	throw ::assets::AssetException("File type not supported");
}
}
