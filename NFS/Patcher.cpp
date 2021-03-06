#include "Patcher.h"
#include "Timer.h"
#include <future>
#include "Bitset.h"
using namespace nfs;

bool Patcher::patch(std::string original, std::string path, std::string out) {
	Buffer og = readFile(original);
	Buffer ptc = readFile(path);

	if (og.size == 0) {
		if (ptc.size != 0)
			deleteBuffer(&ptc);
		return false;
	}

	if (ptc.size == 0) {
		if (og.size != 0)
			deleteBuffer(&og);
		return false;
	}

	Buffer output = patch(og, ptc);
	if (output.size == 0) {
		deleteBuffer(&og);
		deleteBuffer(&ptc);
		return false;
	}

	bool b = writeBuffer(output, out);
	deleteBuffer(&output);
	deleteBuffer(&og);
	deleteBuffer(&ptc);
	return b;
}

Buffer Patcher::patch(Buffer original, Buffer patch) {

	oi::Timer t;

	NFSP_Header *head = (NFSP_Header*)patch.data;

	static const char magicNum[4] = { 'N', 'F', 'S', 'P' };

	if (memcmp(head, magicNum, 4) != 0) {
		printf("Couldn't patch file! Patch was invalid\n");
		return { nullptr, 0 };
	}

	Buffer output = newBuffer1(head->size);
	u32 len = original.size > head->size ? head->size : original.size;
	memcpy(output.data, original.data, len);

	u8 *off = (u8*)(head + 1);

	std::vector<NFSP_Register> registers(head->registers);
	for (u32 i = 0; i < head->registers; ++i) {
		memcpy(&registers[i], off, sizeof(NFSP_Register));
		off += sizeof(NFSP_Register);
	}

	u32 bitsetLength = (u32)ceil(head->blocks / 4.f);
	oi::Bitset types({ off, bitsetLength }, head->blocks * 2);
	off += bitsetLength;

	u32 reg = 0, regC = 0, regtarg = registers[0].count, regval = registers[0].size, prevOff = 0;
	for (u32 i = 0; i < head->blocks; ++i) {

		if (regC >= regtarg - 1) {
			++reg;
			regC = 0;
			regtarg = registers[reg].count;
			regval = registers[reg].size;
		}

		NFSP_Block *block = (NFSP_Block*)off;

		oi::Bitset subset = types.subset(i * 2U, 2U);
		u32 size = subset[0] ? 4 : (subset[1] ? 2 : 1);
		u32 toff = 0;

		if (size == 4) toff = *(u32*)block;
		else if (size == 2) toff = *(u16*)block;
		else toff = *(u8*)block;

		if (size == 4) prevOff = toff;
		else prevOff += toff;
		
		memcpy(output.data + prevOff, off + size, regval);

		off += size + regval;
		++regC;
	}

	t.stop();
	printf("Finished patching a buffer:\n");
	t.print();

	return output;
}

bool Patcher::compare(Buffer a, Buffer b, std::vector<Buffer> &result, u32 &compares) {

	if (a.size != b.size) return false;

	u32 division = a.size / 4;
	u32 remainder = a.size % 4;

	bool res = true;
	for (u32 i = 0; i < 4; ++i) {
		u32 size = division + (i == 3 ? remainder : 0);

		Buffer iA = { a.data + division * i, size };
		Buffer iB = { b.data + division * i, size };

		++compares;
		if (memcmp(iA.data, iB.data, iA.size) == 0) continue;

		res = false;

		if (size <= 1024U) {

			Buffer buf = { nullptr, 0 };
			bool match = true;
			for (u32 j = 0; j < size; ++j) {

				bool curr = iA.data[j] == iB.data[j];

				if (match && !curr) 				//Start of buffer
					buf = { iB.data + j, size - j };
				else if(curr && !match){			//End of buffer
					buf.size = (u32)((iB.data + j) - buf.data);
					result.push_back(buf);
					buf = { nullptr, 0 };
				}

				match = curr;
			}

			if (!match) {							//End of buffer
				buf.size = (u32)((iB.data + size) - buf.data);
				result.push_back(buf);
			}
			
			continue;
		} else
			compare(iA, iB, result, compares);
	}

	return res;
}

Buffer Patcher::writePatch(Buffer original, Buffer modified) {

	oi::Timer t;

	static const char magicNum[4] = { 'N', 'F', 'S', 'P' };

	NFSP_Header header;
	memcpy(&header, magicNum, 4);
	header.size = modified.size;

	u32 end;
	std::vector<NFSP_Block> blocks;

	if (modified.size > original.size) {
		blocks.push_back({ original.size, modified.size - original.size, offset(modified, original.size) });
		end = original.size;
	}
	else
		end = modified.size;

	u32 thrs;
	std::vector<std::future<std::vector<Buffer>>> futures(thrs = std::thread::hardware_concurrency());
	u32 subdivided = end / thrs;
	u32 remainder = end % thrs;

	for (u32 i = 0; i < thrs; ++i) {
		u32 val = subdivided + (i == thrs - 1 ? remainder : 0);
		u32 in = i;
		futures[i] = std::move(std::async([&, in]() -> std::vector<Buffer> {

			std::vector<Buffer> differences;
			u32 compares = 0;

			compare({ original.data + in * subdivided, val }, { modified.data + in * subdivided, val }, differences, compares);

			//printf("Compared %u buffers\n", compares);
			return differences;
		}));
	}

	u32 finalSize = sizeof(NFSP_Header);

	for (u32 i = 0; i < thrs; ++i) {
		std::vector<Buffer> bufs = std::move(futures[i].get());

		if (bufs.size() != 0) {
			u32 off = (u32)blocks.size();
			blocks.resize(off + bufs.size());

			for (u32 j = 0; j < bufs.size(); ++j) {
				Buffer &buf = bufs[j];
				blocks[off + j] = { (u32)(buf.data - modified.data), buf.size, buf };
				finalSize += buf.size;
			}
		}
	}

	if (blocks.size() == 0) {
		printf("Couldn't patch file! As they are identical\n");
		return { nullptr, 0 };
	}

	header.blocks = (u32)blocks.size();

	std::sort(blocks.begin(), blocks.end());

	std::vector<NFSP_Register> regs;

	u32 prevSize = blocks[0].length, prevI = 0;
	for (u32 i = 0; i < blocks.size(); ++i) {

		NFSP_Block *blc = &blocks[i];
		
		if (prevSize != blc->length) {
			regs.push_back({ prevSize, i - prevI + 1});
			prevSize = blc->length;
			prevI = i;
			finalSize += 8;
		}
	}

	oi::Bitset b((u32)blocks.size() * 2);

	prevSize = blocks[0].length;
	u32 prevOff = 0;
	for (u32 i = 0; i < blocks.size(); ++i) {

		NFSP_Block *blc = &blocks[i];

		if (prevSize != blc->length) {
			prevOff = 0;
			prevSize = blc->length;
		}

		u32 off = blc->offset - prevOff;
		if (off < 256) {
			b(oi::Bitset(false, false), 2 * i);
			++finalSize;
		}
		else if (off < 65536) {
			b(oi::Bitset(false, true), 2 * i);
			finalSize += 2;
		}
		else {
			finalSize += 4;
			b(oi::Bitset(true, false), 2 * i);
		}

		prevOff = blc->offset;
	}

	Buffer bbuf = b;
	finalSize += bbuf.size;

	u32 lastI = (u32)blocks.size() - 1;
	auto &last = blocks[lastI];
	if (regs[regs.size() - 1].size != last.length) {
		regs.push_back({ prevSize, lastI - prevI + 1});
		finalSize += 8;
	}

	header.registers = (u32)regs.size();

	Buffer result = newBuffer1(finalSize);
	memcpy(result.data, &header, sizeof(header));

	Buffer offb = offset(result, sizeof(header));

	for (u32 i = 0; i < regs.size(); ++i) {

		NFSP_Register *reg = &regs[i];

		memcpy(offb.data, reg, sizeof(NFSP_Register));

		offb = offset(offb, sizeof(NFSP_Register));
	}

	memcpy(offb.data, bbuf.data, bbuf.size);
	offb = offset(offb, bbuf.size);

	prevSize = blocks[0].length;
	prevOff = 0;
	for (u32 i = 0; i < blocks.size(); ++i) {

		NFSP_Block *blc = &blocks[i];

		u32 off = blc->offset - prevOff;
		if (prevSize != blc->length) {
			off = blc->offset;
			prevSize = blc->length;
		}

		u32 size = 0;

		if (off < 256) {
			*(u8*)offb.data = off;
			size = 1;
		}
		else if (off < 65536) {
			*(u16*)offb.data = off;
			size = 2;
		}
		else {
			*(u32*)offb.data = off;
			size = 4;
		}

		prevOff = blc->offset;

		memcpy(offb.data + size, blc->buf.data, blc->length);

		offb = offset(offb, size + blc->length);
	}

	t.stop();
	printf("Completed writing patch:\n");
	t.print();

	return result;
}

bool Patcher::writePatch(std::string original, std::string modified, std::string patch) {
	Buffer og = readFile(original);
	Buffer mod = readFile(modified);

	if (og.size == 0) {
		if (mod.size != 0)
			deleteBuffer(&mod);
		return false;
	}

	if (mod.size == 0) {
		if (og.size != 0)
			deleteBuffer(&og);
		return false;
	}

	Buffer res = writePatch(og, mod);
	if (res.size == 0) {
		deleteBuffer(&mod);
		deleteBuffer(&og);
		return false;
	}

	bool b = writeBuffer(res, patch);
	deleteBuffer(&res);
	deleteBuffer(&og);
	deleteBuffer(&mod);
	return b;
}