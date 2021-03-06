#include "Types.h"
#include <stdlib.h>
#include <stdio.h>
#include "API/stbi/stbi_write.h"

void deleteBuffer(Buffer *b) {
	if (b->data != NULL) {
		free(b->data);
		b->data = NULL;
		b->size = 0;
	}
}

Buffer newBuffer1(u32 size) {
	Buffer b;
	b.data = (u8*)malloc(size);
	memset(b.data, 0, size);
	b.size = size;
	return b;
}

Buffer newBuffer2(u8 *ptr, u32 size) {
	Buffer b;
	b.data = ptr;
	b.size = size;
	return b;
}

Buffer newBuffer3(u8 *ptr, u32 size) {
	Buffer b = newBuffer1(size);
	copyBuffer(b, { ptr, size }, size, 0);
	return b;
}

bool copyBuffer(Buffer dest, Buffer src, u32 size, u32 offset) {
	if (dest.size < offset + size) return false;
	memcpy(dest.data + offset, src.data, size);
	return true;
}

Buffer offset(Buffer b, u32 off) {
	Buffer res;
	res.size = 0;
	res.data = NULL;
	if (b.size < off) return res;
	res.size = b.size - off;
	res.data = b.data + off;
	return res;
}

u32 getTile(Texture2D t) {
	u32 tiles = t.tt & 0xF0;
	return tiles == 0 ? 0 : 8;
}

bool setUInt(Buffer b, u32 offset, u32 value) {
	if (b.size < offset + 4) return false;

	u32 *ptr = (u32*)(b.data + offset);
	ptr[0] = value;
	return true;
}

bool setUShort(Buffer b, u32 offset, u16 value) {
	if (b.size < offset + 2) return false;

	u16 *ptr = (u16*)(b.data + offset);
	ptr[0] = value;
	return true;
}

u16 getUShort(Buffer b, u32 offset) {
	if (b.size < offset + 2) return 0;
	u16 *ptr = (u16*)(b.data + offset);
	return ptr[0];
}

u32 getUInt(Buffer b) {
	if (b.size < 4) return 0;
	u32 *ptr = (u32*)b.data;
	return ptr[0];
}

Buffer readFile(std::string str) {
	Buffer nullbuf;
	nullbuf.data = NULL;
	nullbuf.size = 0;

	FILE *f = fopen(str.c_str(), "rb");
	if (f == NULL) {
		printf("Couldn't open file (%s)\n", str.c_str());
		return nullbuf;
	}

	fseek(f, 0, SEEK_END);
	u32 fsize = (u32)ftell(f);
	fseek(f, 0, SEEK_SET);

	Buffer res = newBuffer1(fsize);
	fread(res.data, res.size, 1, f);
	fclose(f);

	return res;
}

char hexChar(u8 i) {
	if (i < 10) return '0' + i;
	return 'A' + (i - 10);
}

std::string toHex(u8 i) {
	std::string str = std::string(' ', 3);
	str[0] = hexChar((i & 0xF0) >> 4);
	str[1] = hexChar(i & 0xF);
	str[2] = 0;
	return str;
}

std::string toHex16(u16 i) {
	std::string str = std::string(' ', 5);
	str[0] = hexChar((i & 0xF000) >> 12);
	str[1] = hexChar((i & 0xF00) >> 8);
	str[2] = hexChar((i & 0xF0) >> 4);
	str[3] = hexChar(i & 0xF);
	str[4] = 0;
	return str;
}

std::string toHex32(u32 i) {
	std::string str(' ', 9);
	str[0] = hexChar((i & 0xF0000000) >> 28);
	str[1] = hexChar((i & 0xF000000) >> 24);
	str[2] = hexChar((i & 0xF00000) >> 20);
	str[3] = hexChar((i & 0xF0000) >> 16);
	str[4] = hexChar((i & 0xF000) >> 12);
	str[5] = hexChar((i & 0xF00) >> 8);
	str[6] = hexChar((i & 0xF0) >> 4);
	str[7] = hexChar(i & 0xF);
	str[8] = 0;
	return str;
}

void printBuffer(Buffer b) {
	printf("Buffer with size %u and contents:\n", b.size);
	for (u32 i = 0; i < b.size; ++i) {
		std::string str = toHex(b.data[i]);
		printf("%s ", str.c_str());
		if((i + 1) % 32 == 0)
			printf("\n");
	}
	printf("\n");
}

void clearBuffer(Buffer b) {
	if (b.data == NULL) return;
	memset(b.data, 0, b.size);
}

bool writeBuffer(Buffer b, std::string path) {
	FILE *f = fopen(path.c_str(), "wb");
	if (f == NULL) {
		printf("Couldn't open file for writing (%s)\n", path.c_str());
		return false;
	}

	fwrite(b.data, b.size, 1, f);
	fclose(f);
	printf("Successfully wrote %u bytes to file (%s)\n", b.size, path.c_str());

	return true;
}

bool writeTexture(Texture2D t, std::string path) {
	
	bool conversion = t.stride != 4, result = true;

	if (conversion) 
		t = convertToRGBA8(t);

	if (!stbi_write_png(path.c_str(), t.width, t.height, t.stride, t.data, t.width * t.stride)) {
		printf("Couldn't write texture at \"%s\"\n", path.c_str());
		result = false;
	}

	if (conversion)
		deleteTexture(&t);

	return result;
}


bool isLittleEndian() {
	u16 x = 1;
	return ((u8*)&x)[0] = 1;
}

u32 fetchData(Texture2D t, u32 i, u32 j) {

	if (t.size == 0 || t.data == nullptr || t.stride > 4) return 0;

	u32 tiles = t.tt & 0xF0;
	bool tiled = tiles != 0;
	bool fourBit = (t.tt & 0xF00) == B4;

	u32 index = (j * t.width + i);

	if (tiles == TILED8) {
		u32 tx = i / 8;
		u32 ty = j / 8;
		u32 tix = i % 8;
		u32 tiy = j % 8;

		u32 pt = ty * (t.width / 8) + tx;
		u32 pit = tiy * 8 + tix;
		index = pt * 64 + pit;
	}

	u8 *dat = t.data + index * t.stride / (fourBit ? 2 : 1);
	u32 at = 0;

	for (u32 i = 0; i < t.stride; ++i)
		at |= dat[i] << i * 8;

	if (fourBit)
		if (index % 2 == 0)
			at &= 0xF;
		else
			at = (at & 0xF0) >> 4;

	return at;
}

bool storeData(Texture2D t, u32 i, u32 j, u32 val) {

	if (t.data == nullptr || t.size == 0 || i >= t.width || j >= t.height) return false;

	u32 tiles = t.tt & 0xF0;
	bool tiled = tiles != 0;
	bool fourBit = (t.tt & 0xF00) == B4;

	u32 index = (j * t.width + i);

	if (tiles == TILED8) {
		u32 tx = i / 8;
		u32 ty = j / 8;
		u32 tix = i % 8;
		u32 tiy = j % 8;

		u32 pt = ty * (t.width / 8) + tx;
		u32 pit = tiy * 8 + tix;
		index = pt * 64 + pit;
	}

	u8 *dat = t.data + index * t.stride / (fourBit ? 2 : 1);

	if (fourBit) {
		if (index % 2 == 0)
			*dat = (*dat & 0xF0) | (val & 0xF);
		else
			*dat = (*dat & 0xF) | ((val & 0xF) << 4);
	} else 
		for (u32 i = 0; i < t.stride; ++i)
			dat[i] = ((u8*)&val)[i];

	return true;
}

u32 getPixel(Texture2D t, u32 i, u32 j) {
	u32 dat = fetchData(t, i, j);

	if ((t.tt & 0xF) == BGR5) {
		u32 b = (u32)(((dat & 0x001F) >>  0) / 31.f * 255);
		u32 g = (u32)(((dat & 0x03E0) >>  5) / 31.f * 255);
		u32 r = (u32)(((dat & 0x7C00) >> 10) / 31.f * 255);
		dat = (0xFF << 24) | (r << 16) | (g << 8) | b;
	}

	return dat;
}


bool setPixel(Texture2D t, u32 i, u32 j, u32 val) {

	if ((t.tt & 0xF) == BGR5) {
		u32 b = (u32)ceil(((val & 0xFF0000) >> 16) / 255 * 31.f);
		u32 g = (u32)ceil(((val & 0x00FF00) >>  8) / 255 * 31.f);
		u32 r = (u32)ceil(((val & 0x0000FF) >>  0) / 255 * 31.f);
		val = b | (g << 5) | (r << 10);
	}

	return storeData(t, i, j, val);
}

Texture2D newTexture1(u32 width, u32 height, u32 stride, TextureType tt) {
	Texture2D t = { width * height * stride, width, height, stride, tt, (u8*)malloc(width * height * stride) };
	memset(t.data, 0, t.size);
	return t;
}

Texture2D newTexture2(u8 *ptr, u32 width, u32 height, u32 stride, TextureType tt) {
	return { width * height * stride, width, height, stride, tt, ptr };
}


Texture2D convertToRGBA8(Texture2D t) {
	return runPixelShader(getPixel, t, t.width, t.height);
}

Texture2D convertPT2D(PaletteTexture2D pt2d) {
	return runPixelShader<PaletteTexture2D>([](PaletteTexture2D t, u32 i, u32 j) -> u32 { 
		u32 sample = getPixel(t.tilemap, i, j);
		u32 x = sample & 0xF;
		u32 y = (sample & 0xF0) >> 4;
		return getPixel(t.palette, x, y);
	}, pt2d, pt2d.tilemap.width, pt2d.tilemap.height);
}

Texture2D convertTT2D(TiledTexture2D tt2d) {
	
	const u32 tc = getTile(tt2d.tilemap);

	return runPixelShader<TiledTexture2D>([](TiledTexture2D t, u32 i, u32 j) -> u32 {

		const u32 tc = getTile(t.tilemap);
		const u32 &tw = t.map.width;
		const u32 &th = t.map.height;

		u32 mx = i / tc;							//X index in map
		u32 my = j / tc;							//Y index in map

		u32 tx = i % tc;							//X index in tile
		u32 ty = j % tc;							//Y index in tile

		u32 ptd = getPixel(t.map, mx, my);			//Data of map tile
		u32 ptt = (ptd & 0x0C00) >> 10;				//Translate
		u32 ptp = (ptd & 0xF000) >> 12;				//Palette
		u32 ptm = (ptd & 0x03FF) >>  0;				//Position in tilemap

		u32 ptmx = ptm % tw;						//X tile position in tilemap
		u32 ptmy = ptm / tw;						//Y tile position in tilemap

		if (ptt & 0b10)
			ty = (tc - 1) - ty;

		if (ptt & 0b1)
			tx = (tc - 1) - tx;

		u32 tmx = ptmx * tc + tx;					//X position in tilemap
		u32 tmy = ptmy * tc + ty;					//Y position in tilemap

		u32 tms = getPixel(t.tilemap, tmx, tmy);	//Tilemap sample
		u32 tmsx = tms % t.palette.width;
		u32 tmsy = tms / t.palette.width;

		return getPixel(t.palette, tmsx, ptp | tmsy);

	}, tt2d, tt2d.map.width * tc, tt2d.map.height * tc);
}

void deleteTexture(Texture2D *t) {
	if (t->data != nullptr) {
		free(t->data);
		memset(t, 0, sizeof(*t));
	}
}

void printTexture(Texture2D t, bool printContents) {
	printf("Texture with width %u, height %u, stride %u, type %u and %u bytes of data\n", t.width, t.height, t.stride, t.tt, t.size);
	if (printContents)
		printBuffer({t.data, t.size});
}