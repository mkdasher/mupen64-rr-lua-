#include <Windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../r4300/r4300.h"
#include "memory.h"
#include "../main/guifuncs.h"

#include "summercart.h"

#include "savestates.h"

struct vhd
{
	char cookie[8];
	unsigned long feature;
	unsigned long version;
	unsigned long long next;
	unsigned long stamp;
	char creator_app[4];
	unsigned long creator_version;
	char creator_os[4];
	unsigned long long disk_size;
	unsigned long long data_size;
	unsigned short ncylinder;
	unsigned char nhead;
	unsigned char nsector;
	unsigned long type;
	unsigned long checksum;
	char guid[16];
	unsigned char saved_state;
	char pad[427];
};

#ifdef _BIG_ENDIAN
#define vhd_16(val) (val)
#define vhd_32(val) (val)
#define vhd_64(val) (val)
#else
#define vhd_16(val) _byteswap_ushort(val)
#define vhd_32(val) _byteswap_ulong(val)
#define vhd_64(val) _byteswap_uint64(val)
#endif

static void vhd_copy(struct vhd *vhd, FILE *dst, FILE *src, void *buf, unsigned int n)
{
	unsigned long long len;
	fseek(src, -512, SEEK_END);
	fread(vhd, 1, sizeof(struct vhd), src);
	fseek(src, 0, SEEK_SET);
	for (len = vhd_64(vhd->disk_size)/512; len > 0; len -= n)
	{
		if (n > len) n = len;
		fread(buf, n, 512, src);
		fwrite(buf, n, 512, dst);
	}
}

struct summercart summercart;

static char *get_sd_path()
{
	auto saves_path = get_saves_directory();
	char *path;
	if ((path = (char *)malloc(strlen(saves_path.string().c_str())+8+1)))
	{
		strcpy(path, saves_path.string().c_str());
		strcat(path, "card.vhd");
	}
	return path;
}

static char *get_st_path(const char *filename)
{
	char *path;
	if ((path = (char *)malloc(strlen(filename)+4+1)))
	{
		strcpy(path, filename);
		strcat(path, ".vhd");
	}
	return path;
}

static int sd_error(const char *text, const char *caption)
{
	MessageBox(NULL, text, caption, MB_OK|MB_ICONERROR|MB_TOPMOST);
	return -1;
}

static int sd_seek(FILE *fp, const char *caption)
{
	struct vhd vhd;
	unsigned long sector = summercart.sd_sector;
	unsigned long count = summercart.data1;
	if (fseek(fp, -512, SEEK_END)) return sd_error("Seek(1) error.", caption);
	if (fread(&vhd, 1, sizeof(struct vhd), fp) != sizeof(struct vhd)) return sd_error("Read error.", caption);
	if (memcmp(vhd.cookie, "conectix", 8)) return sd_error("Invalid VHD file.", caption);
	if (vhd_32(vhd.type) != 2) return sd_error("Invalid VHD type: must be a fixed disk.", caption);
	if ((long long)sector+count > vhd_64(vhd.disk_size)/512) return -1;
	if (fseek(fp, 512*(long long)sector, SEEK_SET)) return sd_error("Seek(2) error.", caption);
	return 0;
}

static void sd_read()
{
	unsigned long i;
	FILE *fp;
	char *path;
	char *ptr = NULL;
	unsigned long addr = summercart.data0 & 0x1fffffff;
	unsigned long count = summercart.data1;
	unsigned long size = 512*count;
	if (count > 131072) return;
	if ((path = get_sd_path()))
	{
		if ((fp = fopen(path, "rb")))
		{
			char s = S8;
			if (!sd_seek(fp, "SD read error"))
			{
				if (addr >= 0x1ffe0000 && addr+size <= 0x1ffe2000)
				{
					addr -= 0x1ffe0000;
					ptr = summercart.buffer;
				}
				if (addr >= 0x10000000 && addr+size <= 0x14000000)
				{
					s ^= summercart.sd_byteswap;
					addr -= 0x10000000;
					ptr = (char *)rom;
				}
			}
			if (ptr)
			{
				for (i = 0; i < size; i++) ptr[(addr+i)^s] = fgetc(fp);
				summercart.status = 0;
			}
			fclose(fp);
		}
		else sd_error("Could not open SD image file.", "SD read error");
		free(path);
	}
	else sd_error("Could not generate SD image path.", "SD read error");
}

static void sd_write()
{
	unsigned long i;
	FILE *fp;
	char *path;
	char *ptr = NULL;
	unsigned long addr = summercart.data0 & 0x1fffffff;
	unsigned long count = summercart.data1;
	unsigned long size = 512*count;
	if (count > 131072) return;
	if ((path = get_sd_path()))
	{
		if ((fp = fopen(path, "r+b")))
		{
			if (!sd_seek(fp, "SD write error"))
			{
				if (addr >= 0x1ffe0000 && addr+size <= 0x1ffe2000)
				{
					addr -= 0x1ffe0000;
					ptr = summercart.buffer;
				}
				if (addr >= 0x10000000 && addr+size <= 0x14000000)
				{
					addr -= 0x10000000;
					ptr = (char *)rom;
				}
			}
			if (ptr)
			{
				for (i = 0; i < size; i++) fputc(ptr[(addr+i)^S8], fp);
				summercart.status = 0;
			}
			fclose(fp);
		}
		else sd_error("Could not open SD image file.", "SD write error");
		free(path);
	}
	else sd_error("Could not generate SD image path.", "SD write error");
}

void save_summercart(const char* filename)
{
	unsigned int n;
	void *buf;
	char *sdp;
	char *stp;
	FILE *sdf;
	FILE *stf;
	struct vhd vhd;
	if ((buf = malloc(512*(n = 128))))
	{
		if ((sdp = get_sd_path()))
		{
			if ((stp = get_st_path(filename)))
			{
				if ((sdf = fopen(sdp, "rb")))
				{
					if ((stf = fopen(stp, "wb")))
					{
						vhd_copy(&vhd, stf, sdf, buf, n);
						fwrite(&summercart, 1, sizeof(struct summercart), stf);
						fwrite(&vhd, 1, sizeof(struct vhd), stf);
						fclose(stf);
					}
					else sd_error("Could not open SD state file.", "Save error");
					fclose(sdf);
				}
				else sd_error("Could not open SD image file.", "Save error");
				free(stp);
			}
			else sd_error("Could not generate SD state path.", "Save error");
			free(sdp);
		}
		else sd_error("Could not generate SD image path.", "Save error");
		free(buf);
	}
	else sd_error("Could not allocate buffer.", "Save error");
}

void load_summercart(const char *filename)
{
	unsigned int n;
	void *buf;
	char *stp;
	char *sdp;
	FILE *stf;
	FILE *sdf;
	struct vhd vhd;
	if ((buf = malloc(512*(n = 128))))
	{
		if ((stp = get_st_path(filename)))
		{
			if ((sdp = get_sd_path()))
			{
				if ((stf = fopen(stp, "rb")))
				{
					if ((sdf = fopen(sdp, "wb")))
					{
						vhd_copy(&vhd, sdf, stf, buf, n);
						fread(&summercart, 1, sizeof(struct summercart), stf);
						fwrite(&vhd, 1, sizeof(struct vhd), sdf);
						fclose(sdf);
					}
					else sd_error("Could not open SD image file.", "Load error");
					fclose(stf);
				}
				else sd_error("Could not open SD state file.", "Load error");
				free(sdp);
			}
			else sd_error("Could not generate SD image path.", "Load error");
			free(stp);
		}
		else sd_error("Could not generate SD state path.", "Load error");
		free(buf);
	}
	else sd_error("Could not allocate buffer.", "Load error");
}

void init_summercart()
{
	memset(&summercart, 0, sizeof(struct summercart));
}

unsigned long read_summercart(unsigned long address)
{
	switch (address & 0xFFFC)
	{
	case 0x00:
		if (summercart.unlock) return summercart.status;
		break;
	case 0x04:
		if (summercart.unlock) return summercart.data0;
		break;
	case 0x08:
		if (summercart.unlock) return summercart.data1;
		break;
	case 0x0C:
		if (summercart.unlock) return 0x53437632;
		break;
	}
	return 0;
}

void write_summercart(unsigned long address, unsigned long value)
{
	switch (address & 0xFFFC)
	{
	case 0x00:
		if (summercart.unlock)
		{
			summercart.status = 0x40000000;
			switch (value)
			{
			case 'c':
				switch (summercart.data0)
				{
				case 1:
					summercart.data1 = summercart.cfg_rom_write;
					summercart.status = 0;
					break;
				case 3:
					summercart.data1 = 0;
					summercart.status = 0;
					break;
				case 6:
					summercart.data1 = 0;
					summercart.status = 0;
					break;
				}
				break;
			case 'C':
				switch (summercart.data0)
				{
				case 1:
					if (summercart.data1)
					{
						summercart.data1 = summercart.cfg_rom_write;
						summercart.cfg_rom_write = 1;
					}
					else
					{
						summercart.data1 = summercart.cfg_rom_write;
						summercart.cfg_rom_write = 0;
					}
					summercart.status = 0;
					break;
				}
				break;
			case 'i':
				switch (summercart.data1)
				{
				case 0:
					summercart.status = 0;
					break;
				case 1:
					summercart.status = 0;
					break;
				case 4:
					summercart.sd_byteswap = 1;
					summercart.status = 0;
					break;
				case 5:
					summercart.sd_byteswap = 0;
					summercart.status = 0;
					break;
				}
				break;
			case 'I':
				summercart.sd_sector = summercart.data0;
				summercart.status = 0;
				break;
			case 's':
				sd_read();
				break;
			case 'S':
				sd_write();
				break;
			}
		}
		break;
	case 0x04:
		if (summercart.unlock) summercart.data0 = value;
		break;
	case 0x08:
		if (summercart.unlock) summercart.data1 = value;
		break;
	case 0x10:
		switch (value)
		{
		case 0xFFFFFFFF:
			summercart.unlock = 0;
			break;
		case 0x5F554E4C:
			if (summercart.lock_seq == 0)
			{
				summercart.lock_seq = 2;
			}
			break;
		case 0x4F434B5F:
			if (summercart.lock_seq == 2)
			{
				summercart.unlock = 1;
				summercart.lock_seq = 0;
			}
			break;
		default:
			summercart.lock_seq = 0;
			break;
		}
		break;
	}
}
