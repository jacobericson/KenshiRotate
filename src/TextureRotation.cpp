#include "TextureRotation.h"

#include <Debug.h>

#pragma warning(push)
#pragma warning(disable: 4091)
#include <kenshi/gui/InventoryGUI.h>
#pragma warning(pop)

#include <mygui/MyGUI_ImageBox.h>
#include <mygui/MyGUI_SkinItem.h>

#include <ogre/OgreTextureManager.h>
#include <ogre/OgreTexture.h>
#include <ogre/OgreImage.h>
#include <ogre/OgrePixelFormat.h>
#include <ogre/OgreHardwarePixelBuffer.h>

#include <string>

std::map<std::string, TextureRotation::RotatedTextureInfo> TextureRotation::s_cache;

static Ogre::uint32 nextPowerOf2(Ogre::uint32 v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

bool TextureRotation::GetOrCreateRotatedTexture(const std::string& originalName,
	RotatedTextureInfo& outInfo)
{
	if (originalName.empty())
		return false;

	// Check cache
	std::map<std::string, RotatedTextureInfo>::iterator cacheIt =
		s_cache.find(originalName);
	if (cacheIt != s_cache.end())
	{
		outInfo = cacheIt->second;
		return true;
	}

	std::string rotatedName = originalName + "__rot90";

	// Get the original texture
	Ogre::TexturePtr srcTex = Ogre::TextureManager::getSingleton().getByName(originalName);
	if (srcTex.isNull())
	{
		ErrorLog("[KenshiRotate] Could not find texture: " + originalName);
		return false;
	}

	// Convert texture to an Image for pixel access
	Ogre::Image srcImage;
	srcTex->convertToImage(srcImage);

	Ogre::uint32 srcW = srcImage.getWidth();
	Ogre::uint32 srcH = srcImage.getHeight();
	Ogre::PixelFormat fmt = srcImage.getFormat();
	size_t bpp = Ogre::PixelUtil::getNumElemBytes(fmt);

	if (srcW == 0 || srcH == 0 || bpp == 0)
	{
		ErrorLog("[KenshiRotate] Invalid texture dimensions or format");
		return false;
	}

	// Rotated image dimensions (swapped) and POT-padded texture dimensions
	Ogre::uint32 dstW = srcH;
	Ogre::uint32 dstH = srcW;
	Ogre::uint32 potW = nextPowerOf2(dstW);
	Ogre::uint32 potH = nextPowerOf2(dstH);

	// Allocate POT buffer, zero-filled (transparent black padding)
	size_t potBufSize = potW * potH * bpp;
	Ogre::uchar* potData = new Ogre::uchar[potBufSize];
	memset(potData, 0, potBufSize);

	// Rotate 90 CW: src(x, y) -> dst(srcH - 1 - y, x)
	// Write into top-left of POT buffer using potW as row stride
	const Ogre::uchar* srcData = srcImage.getData();
	size_t srcRowPitch = srcImage.getRowSpan();

	for (Ogre::uint32 y = 0; y < srcH; y++)
	{
		for (Ogre::uint32 x = 0; x < srcW; x++)
		{
			Ogre::uint32 dstX = srcH - 1 - y;
			Ogre::uint32 dstY = x;

			const Ogre::uchar* srcPixel = srcData + y * srcRowPitch + x * bpp;
			Ogre::uchar* dstPixel = potData + dstY * (potW * bpp) + dstX * bpp;
			memcpy(dstPixel, srcPixel, bpp);
		}
	}

	// Check if rotated texture already exists in Ogre (e.g. from a previous session)
	if (Ogre::TextureManager::getSingleton().getByName(rotatedName).isNull())
	{
		// Create POT texture and upload pixels via lock/unlock.
		// Avoid loadImage/blitFromMemory which deadlock from the UI thread.
		try
		{
			Ogre::TexturePtr rotTex = Ogre::TextureManager::getSingleton().createManual(
				rotatedName,
				Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
				Ogre::TEX_TYPE_2D,
				potW, potH, 0, fmt,
				Ogre::TU_STATIC_WRITE_ONLY);

			Ogre::HardwarePixelBufferSharedPtr buf = rotTex->getBuffer();
			void* dest = buf->lock(Ogre::HardwareBuffer::HBL_DISCARD);

			// POT texture — row pitch should match potW, but use buf width to be safe
			size_t gpuRowBytes = buf->getWidth() * bpp;
			size_t srcRowBytes = potW * bpp;
			size_t copyBytes = gpuRowBytes < srcRowBytes ? gpuRowBytes : srcRowBytes;
			if (gpuRowBytes != srcRowBytes)
				ErrorLog("[KenshiRotate] GPU row pitch mismatch");
			for (Ogre::uint32 row = 0; row < potH; row++)
			{
				memcpy(
					(Ogre::uchar*)dest + row * gpuRowBytes,
					potData + row * srcRowBytes,
					copyBytes);
			}

			buf->unlock();
		}
		catch (Ogre::Exception& e)
		{
			ErrorLog("[KenshiRotate] Failed to create rotated texture: "
				+ std::string(e.what()));
			delete[] potData;
			return false;
		}
	}

	delete[] potData;

	outInfo.name = rotatedName;
	outInfo.uMax = (float)dstW / (float)potW;
	outInfo.vMax = (float)dstH / (float)potH;
	s_cache[originalName] = outInfo;

	return true;
}

void TextureRotation::ApplyRotatedTexture(InventoryIcon* icon)
{
	if (!icon || !icon->image)
		return;

	std::string texName = ((MyGUI::SkinItem*)icon->image)->_getTextureName();
	// Don't re-rotate an already-rotated texture
	if (texName.find("__rot90") != std::string::npos)
		return;

	RotatedTextureInfo info;
	if (GetOrCreateRotatedTexture(texName, info))
	{
		// Set texture at the SkinItem level, then crop UVs to the actual
		// image area within the POT texture via the SubSkin.
		((MyGUI::SkinItem*)icon->image)->_setTextureName(info.name);
		MyGUI::ISubWidgetRect* main =
			((MyGUI::SkinItem*)icon->image)->getSubWidgetMain();
		if (main)
			main->_setUVSet(MyGUI::FloatRect(0, 0, info.uMax, info.vMax));
	}
}

void TextureRotation::RestoreOriginalTexture(InventoryIcon* icon)
{
	if (!icon || !icon->image)
		return;

	std::string texName = ((MyGUI::SkinItem*)icon->image)->_getTextureName();
	size_t pos = texName.find("__rot90");
	if (pos != std::string::npos)
	{
		std::string originalName = texName.substr(0, pos);
		// Restore texture and reset UVs to full original texture
		((MyGUI::SkinItem*)icon->image)->_setTextureName(originalName);
		MyGUI::ISubWidgetRect* main =
			((MyGUI::SkinItem*)icon->image)->getSubWidgetMain();
		if (main)
			main->_setUVSet(MyGUI::FloatRect(0, 0, 1, 1));
	}
}
