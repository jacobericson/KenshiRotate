#pragma once

#include <map>
#include <string>

class InventoryIcon;

class TextureRotation
{
public:
	static void ApplyRotatedTexture(InventoryIcon* icon);
	static void RestoreOriginalTexture(InventoryIcon* icon);

private:
	struct RotatedTextureInfo
	{
		std::string name;
		float uMax;
		float vMax;
	};

	static bool GetOrCreateRotatedTexture(const std::string& originalName,
		RotatedTextureInfo& outInfo);

	static std::map<std::string, RotatedTextureInfo> s_cache;
};
