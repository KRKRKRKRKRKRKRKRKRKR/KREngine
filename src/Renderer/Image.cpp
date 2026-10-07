#include "Renderer/Image.h"

#include "KR/Core/Log.h"

// stb_image の本体(関数の中身)を、このファイルに、取り込む(ほかのファイルでは、これを書かない)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fstream>
#include <iterator>

namespace KR
{
    ImageData LoadImageRgba8(const std::filesystem::path& file)
    {
        // ① ファイルを、まるごと、メモリに読む
        // (stb の stbi_load に、パスを渡すと、日本語などの文字が入ったパスで、開けないことがある。std::filesystem で開けば、問題ない)
        std::ifstream stream(file, std::ios::binary);
        if (!stream.is_open())
        {
            KR_LOG_ERROR("Image: ファイルを開けません: {}", file.string());
            return {};
        }
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

        // ② ほどく。最後の「4」は、必ず 4 チャンネル(RGBA)にそろえる、という指定
        int width = 0;
        int height = 0;
        int originalChannels = 0; // 元のファイルのチャンネル数(使わない。ログ用)
        stbi_uc* decoded = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &originalChannels, 4);
        if (!decoded)
        {
            KR_LOG_ERROR("Image: {} を画像として読めません: {}", file.string(), stbi_failure_reason());
            return {};
        }

        // ③ stb が確保したメモリから、自分の配列へコピーして、stb のメモリは、すぐ返す
        ImageData image;
        image.width = static_cast<uint32_t>(width);
        image.height = static_cast<uint32_t>(height);
        image.pixels.assign(decoded, decoded + static_cast<size_t>(width) * height * 4);
        stbi_image_free(decoded);

        KR_LOG_INFO("Image: {} {}x{}(元は {} チャンネル)", file.filename().string(), width, height, originalChannels);
        return image;
    }
}
