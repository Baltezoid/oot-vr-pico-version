#include "TextureExporter.h"
#include "../ZAPD/ZFile.h"

void OTRExporter_Texture::Save(ZResource* res, const fs::path& outPath, BinaryWriter* writer)
{
	ZTexture* tex = (ZTexture*)res;
	
	WriteHeader(tex, outPath, writer, static_cast<uint32_t>(Fast::ResourceType::Texture));

	auto start = std::chrono::steady_clock::now();

	//printf("Exporting Texture %s\n", tex->GetName().c_str());
	
	writer->Write((uint32_t)tex->GetTextureType());
	writer->Write((uint32_t)tex->GetWidth());
	writer->Write((uint32_t)tex->GetHeight());

	writer->Write((uint32_t)tex->GetRawDataSize());

	if (tex->parent != nullptr) {
 		auto data = tex->parent->GetRawData();
 		writer->Write((char*)data.data() + tex->GetRawDataIndex(), tex->GetRawDataSize());
 	}

	auto end = std::chrono::steady_clock::now();
	size_t diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	//printf("Exported Texture %s in %zums\n", tex->GetName().c_str(), diff);

	//if (diff > 2)
		//printf("Export took %lms\n", diff);
}

// SOH [Quest] The texture is `scale` times the size that the game draws, as the textures of HD mods. The caller writes
// the image data after the header.
void OTRExporter_Texture::SaveHdHeader(ZTexture* tex, float scale, BinaryWriter* writer)
{
	WriteHeader(tex, "", writer, static_cast<uint32_t>(Fast::ResourceType::Texture), 1);

	writer->Write((uint32_t)tex->GetTextureType());
	writer->Write((uint32_t)tex->GetWidth());
	writer->Write((uint32_t)tex->GetHeight());
	writer->Write((uint32_t)1); // TEX_FLAG_LOAD_AS_RAW
	writer->Write(scale); // HByteScale
	writer->Write(scale); // VPixelScale
	writer->Write((uint32_t)tex->GetRawDataSize());
}
