
// read thumbnail from Photoshop PSD file
//
// license: public domain
//
// cf. http://www.adobe.com/devnet-apps/photoshop/fileformatashtml/

#include <stdint.h>
#include <stdio.h>
#include <vector>

inline uint32_t read_uint32_be(void const *p)
{
	uint8_t const *q = (uint8_t const *)p;
	return (q[0] << 24) | (q[1] << 16) | (q[2] << 8) | q[3];
}

inline uint16_t read_uint16_be(void const *p)
{
	uint8_t const *q = (uint8_t const *)p;
	return (q[0] << 8) | q[1];
}

bool readPhotoshopThumbnail(char const *path, std::vector<char> *jpeg)
{
	jpeg->clear();

	FILE *fp = fopen(path, "rb");
	if (fp) {
		struct FileHeader {
			char sig[4];
			char ver[2];
			char reserved[6];
			char channels[2];
			char height[4];
			char width[4];
			char depth[2];
			char colormode[2];
		};
		FileHeader fh;
		fread((char *)&fh, 1, sizeof(FileHeader), fp);

		if (memcmp(fh.sig, "8BPS", 4) == 0) {
			char tmp[4];
			uint32_t len;
			fread(tmp, 1, 4, fp);
			len = read_uint32_be(tmp);
			fseek(fp, len, SEEK_CUR);
			fread(tmp, 1, 4, fp);
			len = read_uint32_be(tmp);

			while (1) {
				struct ImageResourceHeader {
					char sig[4];
					char id[2];
				};
				ImageResourceHeader irh;
				fread((char *)&irh, 1, sizeof(ImageResourceHeader), fp);
				if (memcmp(irh.sig, "8BIM", 4) == 0) {
					std::vector<char> name;
					while (1) {
						int c = getc(fp);
						if (c < 0) break;
						if (c == 0) {
							if ((name.size() & 1) == 0) {
								if (getc(fp) < 0) break;
							}
							break;
						} else {
							name.push_back(c);
						}
					}

					fread(tmp, 1, 4, fp);
					len = read_uint32_be(tmp);

					long pos = ftell(fp);

					uint16_t resid = read_uint16_be(irh.id);
					if (resid == 0x0409 || resid == 0x040c) {
						struct ThumbnailResourceHeader {
							uint32_t format;
							uint32_t width;
							uint32_t height;
							uint32_t widthbytes;
							uint32_t totalsize;
							uint32_t size_after_compression;
							uint16_t bits_per_pixel;
							uint16_t num_of_planes;
						};
						ThumbnailResourceHeader trh;
						fread((char *)&trh, 1, sizeof(ThumbnailResourceHeader), fp);
						if (read_uint32_be(&trh.format) == 1) {
							uint32_t size_after_compression = read_uint32_be(&trh.size_after_compression);
							if (size_after_compression < 1000000) {
								jpeg->resize(size_after_compression);
								char *ptr = &jpeg->at(0);
								if (fread(ptr, 1, size_after_compression, fp) == size_after_compression) {
									// ok
								} else {
									jpeg->clear();
								}
							}
						}
						break;
					}

					if (len & 1) len++;
					fseek(fp, pos + len, SEEK_SET);
				} else {
					break;
				}
			}
		}
		fclose(fp);
		return true;
	}

	return false;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: psdthumb <input.psd> <output.jpg>\n");
		return 1;
	}
	char const *in = argv[1];
	char const *out = argv[2];

	std::vector<char> jpeg;
	if (readPhotoshopThumbnail(in, &jpeg) && !jpeg.empty()) {
		FILE *fp = fopen(out, "wb");
		if (fp) {
			fwrite(&jpeg[0], 1, jpeg.size(), fp);
			fclose(fp);
		}
	}

	return 0;
}

