#include <algorithm>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

struct BitfieldMasks
{
    DWORD redMask;
    DWORD greenMask;
    DWORD blueMask;
};

const int PaletteSize = 256;

RGBTRIPLE extract_color(WORD pixel, const BitfieldMasks& masks);
RGBTRIPLE compute_average_color(const std::vector<RGBTRIPLE>& pixels);
void median_cut(const std::vector<RGBTRIPLE>& image, std::vector<RGBQUAD>& palette, int depth);
void median_cut_recursive(std::vector<RGBTRIPLE>& pixels, std::vector<RGBQUAD>& palette, int depth, int begin, int end);
BYTE find_nearest_color_index(const RGBTRIPLE& pixel, const std::vector<RGBQUAD>& palette);

int main()
{
    wstring inputFile = L"input.bmp", outputFile = L"output.bmp";
    BITMAPFILEHEADER bmpFileHeader;
    BITMAPINFOHEADER bmpInfoHeader;
    int Width, Height;
    DWORD RW;
    BitfieldMasks masks;

    HANDLE hInputFile = CreateFile(inputFile.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0,
                                   nullptr);
    if (hInputFile == INVALID_HANDLE_VALUE)
    {
        wcout << L"Can't open file " << inputFile << "\n";
        return -1;
    }

    HANDLE hOutFile = CreateFile(outputFile.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (hOutFile == INVALID_HANDLE_VALUE)
    {
        wcout << L"Can't create file " << outputFile << "\n";
        CloseHandle(hInputFile);
        return -1;
    }

    ReadFile(hInputFile, &bmpFileHeader, sizeof(bmpFileHeader), &RW, nullptr);
    ReadFile(hInputFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &RW, nullptr);

    if (bmpInfoHeader.biCompression == BI_BITFIELDS)
    {
        ReadFile(hInputFile, &masks, sizeof(BitfieldMasks), &RW, NULL);
    }
    else
    {
        masks.redMask = 0xF800;
        masks.greenMask = 0x07E0;
        masks.blueMask = 0x001F;
    }

    SetFilePointer(hInputFile, bmpFileHeader.bfOffBits, NULL, FILE_BEGIN);
    Width = bmpInfoHeader.biWidth;
    Height = bmpInfoHeader.biHeight;

    bmpFileHeader.bfOffBits = sizeof(bmpFileHeader) + sizeof(bmpInfoHeader) + 1024;
    bmpInfoHeader.biBitCount = 8;

    LONG imagePadding = Height * (3 * Width % 4);
    bmpFileHeader.bfSize = bmpFileHeader.bfOffBits + Width * Height + imagePadding;

    WriteFile(hOutFile, &bmpFileHeader, sizeof(bmpFileHeader), &RW, nullptr);
    WriteFile(hOutFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &RW, nullptr);

    vector<RGBTRIPLE> image;
    image.reserve(Width * Height);

    WORD* rowBuffer = new WORD[bmpInfoHeader.biWidth];
    for (int i = 0; i < Height; i++)
    {
        ReadFile(hInputFile, rowBuffer, Width * sizeof(WORD), &RW, nullptr);

        for (int j = 0; j < bmpInfoHeader.biWidth; j++)
            image.push_back(extract_color(rowBuffer[j], masks));

        SetFilePointer(hInputFile, Width % 4, nullptr, FILE_CURRENT);
    }

    vector<RGBQUAD> Palette;
    Palette.reserve(256);

    // 256 colors = 8 depth
    median_cut(image, Palette, 8);

    WriteFile(hOutFile, Palette.data(), 256 * sizeof(RGBQUAD), &RW, nullptr);

    BYTE* outRowBuf = new BYTE[bmpInfoHeader.biWidth];
    BYTE* paddingZeros = new BYTE[imagePadding];

    BYTE colorLUT[65536];
    for (int i = 0; i < 65536; i++)
    {
        RGBTRIPLE pixel;
        pixel.rgbtRed = (i >> 11) * 255 / 31;
        pixel.rgbtGreen = (i >> 5 & 0x3F) * 255 / 63;
        pixel.rgbtBlue = (i & 0x1F) * 255 / 31;

        colorLUT[i] = find_nearest_color_index(pixel, Palette);
    }

    BYTE* outputBuffer = new BYTE[Width * Height + imagePadding];
    BYTE* bufferPtr = outputBuffer;

    for (int i = 0; i < Height; i++)
    {
        for (int j = 0; j < Width; j++)
        {
            WORD colorKey = (image[i * Width + j].rgbtRed >> 3) << 11 |
                            (image[i * Width + j].rgbtGreen >> 2) << 5 |
                            (image[i * Width + j].rgbtBlue >> 3);
            *bufferPtr++ = colorLUT[colorKey];
        }
        memset(bufferPtr, 0, imagePadding);
        bufferPtr += imagePadding;
    }

    WriteFile(hOutFile, outputBuffer, Width * Height + imagePadding, &RW, nullptr);
    delete[] outputBuffer;
    
    delete[] paddingZeros;
    delete[] rowBuffer;

    CloseHandle(hInputFile);
    CloseHandle(hOutFile);

    wcout << "Updating has come to the end successfully!";
    system("pause");
}

RGBTRIPLE extract_color(WORD pixel, const BitfieldMasks& masks)
{
    RGBTRIPLE color;
    color.rgbtRed = ((pixel & masks.redMask) >> 8) & 0xFF;
    color.rgbtGreen = ((pixel & masks.greenMask) >> 3) & 0xFF;
    color.rgbtBlue = ((pixel & masks.blueMask) << 3) & 0xFF;
    return color;
}

RGBTRIPLE compute_average_color(const std::vector<RGBTRIPLE>& pixels)
{
    if (pixels.size() == 0) return {0, 0, 0};

    int sumR = 0, sumG = 0, sumB = 0;
    for (const auto& px : pixels)
    {
        sumR += px.rgbtRed;
        sumG += px.rgbtGreen;
        sumB += px.rgbtBlue;
    }
    int size = pixels.size();

    RGBTRIPLE avgColor;
    avgColor.rgbtRed = sumR / size;
    avgColor.rgbtGreen = sumG / size;
    avgColor.rgbtBlue = sumB / size;

    return avgColor;
}

void median_cut(const std::vector<RGBTRIPLE>& image, std::vector<RGBQUAD>& palette, int depth)
{
    vector<RGBTRIPLE> image_copy(image);
    median_cut_recursive(image_copy, palette, depth, 0, image_copy.size());
}

void median_cut_recursive(std::vector<RGBTRIPLE>& pixels, std::vector<RGBQUAD>& palette, int depth, int begin, int end)
{
    if (begin >= end) return;
    if (depth == 0)
    {
        RGBQUAD color = {};
        RGBTRIPLE avgColor = compute_average_color({pixels.begin() + begin, pixels.begin() + end});

        color.rgbRed = avgColor.rgbtRed;
        color.rgbGreen = avgColor.rgbtGreen;
        color.rgbBlue = avgColor.rgbtBlue;

        palette.push_back(color);
        return;
    }

    BYTE minR = 255, maxR = 0, minG = 255, maxG = 0, minB = 255, maxB = 0;
    for (int i = begin; i < end; i++)
    {
        minR = min(pixels[i].rgbtRed, minR);
        maxR = max(pixels[i].rgbtRed, maxR);
        minG = min(pixels[i].rgbtGreen, minG);
        maxG = max(pixels[i].rgbtGreen, maxG);
        minB = min(pixels[i].rgbtBlue, minB);
        maxB = max(pixels[i].rgbtBlue, maxB);
    }

    BYTE rangeR = maxR - minR;
    BYTE rangeG = maxG - minG;
    BYTE rangeB = maxB - minB;

    int splitChannel = (rangeR >= rangeG && rangeR >= rangeB) ? 0 : (rangeG >= rangeR && rangeG >= rangeB) ? 1 : 2;

    std::sort(pixels.begin() + begin, pixels.begin() + end, [splitChannel](const RGBTRIPLE& a, const RGBTRIPLE& b)
    {
        if (splitChannel == 0) return a.rgbtRed < b.rgbtRed;
        if (splitChannel == 1) return a.rgbtGreen < b.rgbtGreen;
        return a.rgbtBlue < b.rgbtBlue;
    });

    int medianIndex = (begin + end) / 2;
    median_cut_recursive(pixels, palette, depth - 1, begin, medianIndex);
    median_cut_recursive(pixels, palette, depth - 1, medianIndex, end);
}

// Euclidean distance
BYTE find_nearest_color_index(const RGBTRIPLE& pixel, const vector<RGBQUAD>& palette)
{
    int bestIndex = 0;
    int minDist = INT_MAX;

    for (int i = 0; i < palette.size(); i++)
    {
        int dr = pixel.rgbtRed - palette[i].rgbRed;
        int dg = pixel.rgbtGreen - palette[i].rgbGreen;
        int db = pixel.rgbtBlue - palette[i].rgbBlue;
        int dist = dr * dr + dg * dg + db * db;

        if (dist < minDist)
        {
            minDist = dist;
            bestIndex = i;
        }
    }

    return bestIndex;
}