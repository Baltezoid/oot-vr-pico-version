#pragma once

#include "ZResource.h"
#include "ZTexture.h"
#include "Exporter.h"
#include <Utils/BinaryWriter.h>

class OTRExporter_Texture : public OTRExporter
{
public:
	virtual void Save(ZResource* res, const fs::path& outPath, BinaryWriter* writer) override;
	// SOH [Quest] Writes the header of a Texture V1 with a scale, for the HD textures of the custom assets.
	void SaveHdHeader(ZTexture* tex, float scale, BinaryWriter* writer);
};