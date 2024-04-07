/* translated from WinAPI C code to (almost) POSIX; added big-endian host CPU
   support. POSIX doesn't guarantee struct packing to my knowledge
   Translated by Wyatt Ward, 2024/04/07.

   This version should actually still be buildable for windows using MinGW GCC
   (or maybe even clang).
   To use it with MSVC or Borland or whatever, you will need to at minimum
   change the #pragma's for struct packing to match what your compiler wants.

   Tested on x86 and also 32-bit big-endian PowerPC; needs NHD_IS_BIG_ENDIAN
   to be defined on big-endian platforms.
*/

/* WinAPI code originally by junnno (N. Inoue), 2002/12/08 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SECTOR_LENGTH 512
#define MERGE_BUFFER_SIZE (1024*1024*32)
#define NHD_FILE_ID "T98HDDIMAGE.R0"

/* define NHDGEN_IS_BIG_ENDIAN if building for a big-endian computer */
/* #define NHDGEN_IS_BIG_ENDIAN 1 */

/* define LANG_JAPANESE to print help text in Japanese. */
/* writes English help text if undefined. */
/* #define LANG_JAPANESE 1 */

#ifdef NHDGEN_IS_BIG_ENDIAN
uint32_t nhdgen_32le(uint32_t endian_in) {
  /* this does the same thing as __bswap_32() but can be done without
     non-standard headers. */
  return ((endian_in >> 24) & 0xff) | /* move byte 3 to byte 0 */
    ((endian_in >> 8) & 0xff00) |     /* move byte 2 to byte 1 */
    ((endian_in << 8) & 0xff0000) |   /* move byte 1 to byte 2 */
    ((endian_in << 24) & 0xff000000); /* move byte 0 to byte 3 */
}
uint16_t nhdgen_16le(uint16_t endian_in) {
  return ((endian_in >> 8 & 0xff) |
          endian_in << 8 & 0xff00);
}
#else
uint32_t nhdgen_32le(uint32_t endian_in) {
  return endian_in;
}
uint16_t nhdgen_16le(uint16_t endian_in) {
  return endian_in;
}
#endif

/* IMPORTANT: pack these structures! */
#pragma pack(push, 1)

typedef struct {
  char     szFileID[15];          /* 識別ID "T98HDDIMAGE.R0" */
  char     Reserve1[1];
  char     szComment[0x100];
  uint32_t dwHeadSize;
  uint32_t dwCylinder;
  uint16_t wHead;
  uint16_t wSect;
  uint16_t wSectLen;
  char     Reserve2[2];
  char     Reserve3[0xe0];
} NHD_FILE_HEAD,*LP_NHD_FILE_HEAD;

typedef struct {
  uint8_t  Jump[3];
  uint8_t  OEMName[8];
  uint16_t BytesPerSector;
  uint8_t  SectorsPerCluster;
  uint16_t ReservedSector;
  uint8_t  FATCount;
  uint16_t RootDirEntriesCount;
  uint16_t SectorsCount;
  uint8_t  MediaType;
  uint16_t FATSectors;
  uint16_t BIOSSectors;
  uint16_t BIOSHeads;
  uint32_t HiddenSectors;
  uint32_t DoubleSectorsCount;
  uint8_t  OSData[3];
  uint32_t VolumeSerial;
  uint8_t  VolumeLabel[11];
  uint8_t  FileSystem[8];
} BOOT_RECORD_FAT16;

typedef struct
{
  uint8_t  Jump[3];
  uint8_t  OEMName[8];
  uint16_t BytesPerSector;
  uint8_t  SectorsPerCluster;
  uint16_t ReservedSector;
  uint8_t  FATCount;
  uint16_t RootDirEntriesCount;	/* must be zero */
  uint16_t SectorsCount;			/* must be zero */
  uint8_t  MediaType;
  uint16_t FATSectors;            /* must be zero */
  uint16_t BIOSSectors;
  uint16_t BIOSHeads;
  uint32_t HiddenSectors;
  uint32_t DoubleSectorsCount;

  /* FAT32 */
  uint32_t DoubleFATSectorsCount;
  uint16_t wFlags;
  uint16_t wVersion;
  uint32_t dwRootDirStart;
  uint16_t wInfoStart;
  uint16_t wBackupStart;
  uint8_t  Reserved[12];
  uint8_t  cLogicalDriveNumber;
  uint8_t  cUnused;
  uint8_t  cExtendedSignature;
  uint32_t dwSerialNumber;
  uint8_t  strVolumeName[11];
  uint8_t  strFATName[8];  		/* must be "FAT32" */
  uint8_t  Program[420];
  uint8_t  strSignature[2];
} BOOT_RECORD_FAT32;

#pragma pack(pop)

int32_t getFileSize(FILE *fp) {
  int32_t current;
  int32_t last;

  current = ftell(fp);
  fseek(fp, 0, SEEK_END);
  last = ftell(fp);
  fseek(fp, current, SEEK_SET);

  return last;
}

int32_t autoHeader(const char *srcfile, LP_NHD_FILE_HEAD pnfh) {
  int32_t i;
  char buf[SECTOR_LENGTH];
  FILE *fp;

  fp = fopen(srcfile, "rb");
  if(!fp){
    printf("Source file '%s' cannot be opened.\n", srcfile);
    return 0;
  }

  for(i=0; !feof(fp); i++) {
    if(i % 100 == 0) printf("%d bytes were scanned.\n", i*SECTOR_LENGTH);
    fread(buf, SECTOR_LENGTH, 1, fp);
    /* this should not be endien dependent since the chars are each 8 bits */
    if( (memcmp(((BOOT_RECORD_FAT16*)buf)->FileSystem, "FAT16   ", 8)==0) /* FAT block __ */
        ||(memcmp(((BOOT_RECORD_FAT16*)buf)->FileSystem, "FAT12   ", 8)==0)
        ||(memcmp(((BOOT_RECORD_FAT32*)buf)->strFATName, "FAT32   ", 8)==0))
    {
      BOOT_RECORD_FAT16 *pbr = (BOOT_RECORD_FAT16*)buf;
      printf("Partition boot block found at offset %d.\n", ftell(fp));
      pnfh->wHead = pbr->BIOSHeads;
      pnfh->wSect = pbr->BIOSSectors;
      pnfh->wSectLen = nhdgen_16le(SECTOR_LENGTH);
      pnfh->dwCylinder = nhdgen_32le(getFileSize(fp)/(nhdgen_16le(pnfh->wHead)*nhdgen_16le(pnfh->wSect)*nhdgen_16le(pnfh->wSectLen)));
      printf("Disk parameters are...\nHeads: %u\nSectors: %u\nCylinders: %u\nSector length: %u\n\n",
             nhdgen_16le(pnfh->wHead),
             nhdgen_16le(pnfh->wSect),
             nhdgen_32le(pnfh->dwCylinder),
             nhdgen_16le(pnfh->wSectLen));
      fclose(fp);
      return 1;
    }
  }
  fclose(fp);
  printf("Partition boot block was not found.\n");
  return 0;
}

int32_t manualHeader(LP_NHD_FILE_HEAD pnfh)
{
  char strbuf[0x100];
  uint32_t size;

  printf("Please input number of heads.\n");
  fgets(strbuf, 0x100, stdin);
  pnfh->wHead = nhdgen_16le(atoi(strbuf));
  printf("Input number of sectors.\n");
  fgets(strbuf, 0x100, stdin);
  pnfh->wSect = nhdgen_16le(atoi(strbuf));
  printf("Input sector length. It is 512 bytes typically.\n");
  fgets(strbuf, 0x100, stdin);
  pnfh->wSectLen = nhdgen_16le(atoi(strbuf));
  printf("Input number of cylinders.\n");
  fgets(strbuf, 0x100, stdin);
  pnfh->dwCylinder = nhdgen_32le(atoi(strbuf));
  size = nhdgen_32le(nhdgen_16le(pnfh->wHead)
                     * nhdgen_16le(pnfh->wSect)
                     * nhdgen_16le(pnfh->wSectLen)
                     * nhdgen_32le(pnfh->dwCylinder));
  printf("OK, Image file size is %u %03u %03u bytes.\n\n",
         nhdgen_32le(size)/1000000,
         nhdgen_32le(size)/1000%1000,
         nhdgen_32le(size)%1000);

  return 1;
}

int mergeHeader(LP_NHD_FILE_HEAD pnfh, const char *src, const char *dst)
{
  FILE *fps, *fpd;
  char *buf;
  int result = 1;

  buf = malloc(MERGE_BUFFER_SIZE);
  if(!buf){
    printf("Failed to allocate memory.\n");
    return 0;
  }

  fps = fopen(src, "rb");
  if(fps){
    fpd = fopen(dst, "wb");
    if(fpd){
      int size;
      int i;

      fwrite(pnfh, sizeof(NHD_FILE_HEAD), 1, fpd);
      if(ferror(fpd)){
        printf("Header writing error.\n");
        result = 0;
      }
      else{
        printf("Header write ok.\n");
        for(i=0; !feof(fps);){
          printf("Reading...\n");
          size = fread(buf, 1, MERGE_BUFFER_SIZE, fps);
          if(ferror(fps)){
            printf("Data reading error.\n");
            result = 0;
            break;
          }
          printf("Writing...\n");
          fwrite(buf, size, 1, fpd);
          if(ferror(fpd)){
            printf("Data writing error.\n");
            result = 0;
            break;
          }
          fflush(fpd);
          i+=size;
          printf("%d bytes done.\n", i);
        }
      }
      fclose(fpd);
    }
    fclose(fps);
  }
  free(buf);
  return result;
}

int writeHeader(LP_NHD_FILE_HEAD pnfh, const char* dstfile)
{
  FILE* fp;

  printf("Writing...\n");
  fp = fopen(dstfile, "wb");
  if(fp){
    fwrite(pnfh, sizeof(NHD_FILE_HEAD), 1, fp);
    fclose(fp);
    printf("Done.\n");
    return 1;
  }
  else{
    printf("Failed to open the file '%s'. Inputs were discarded.\n", dstfile);
    return 0;
  }
}

int main(int argc, char **argv)
{
  NHD_FILE_HEAD nfh;
  FILE* fp;
  char strbuf[0x100];
  int i;
  const char *srcfile = "";
  const char *dstfile = "";
  int manual = 1;
  int merge = 0;

  for(i=1; i<argc; i++){
    if(argv[i][0] == '-'){
      if(strcmp(argv[i], "--auto")==0) manual = 0;
      else if(strcmp(argv[i], "--merge")==0) merge = 1;
    }
    else{
      if(!srcfile[0] && (merge || !manual)) srcfile = argv[i];
      else if(!dstfile[0]) dstfile = argv[i];
    }
  }

  if( !dstfile[0] || ((merge || !manual) && !srcfile[0])){
#ifndef LANG_JAPANESE
    printf(
      "Usage: nhdgen [--auto] [--merge] input_image_filename output_filename\n\n"
      "--auto  : Automatic mode. Uses manual mode unless specified.\n"
      "--merge : Combine header and disk image; output NHD file. if not specified,\n"
      "          only a header file will be written out.\n"
      "When using manual mode and not combining, specify only the output filename.\n\n"
      "Example) nhdgen --auto --merge hdd.bin hdd.nhd\n"
      "causes header to automatically be calculated from hdd.bin, and the combined\n"
      "header and disk image are written out to hdd.nhd.\n");
#else
    printf(
      "Usage: nhdgen [--auto] [--merge] イメージファイル名 出力ファイル名\n\n"
      "--auto  : 自動モード。指定しなかった場合は手動モード。\n"
      "--merge : ヘッダとディスクイメージを結合してNHDファイルを出力する。\n"
      "         指定しなかった場合はヘッダのみを出力する。\n"
      "手動モードでかつ結合しない場合は出力ファイル名だけを指定します。\n\n"
      "例) nhdgen --auto --merge hdd.bin hdd.nhd\n"
      "hdd.binからパラメータを自動認識し、ヘッダを結合してhdd.nhdに出力します。\n");
#endif
    return 0;
  }

  printf("Mode: %s\n", manual?"manual":"auto");
  printf("Merge: %s\n", merge?"yes":"no");
  printf("Source file: %s\n", srcfile[0]?srcfile:"(none)");
  printf("Destination file: %s\n", dstfile[0]?dstfile:"(none)");

  /* little endian dependency? Might not matter since source file is little endian */
  memset(&nfh, 0, sizeof(nfh));	/* zero memory */
  memcpy(nfh.szFileID, NHD_FILE_ID, 15);	/* header signature */
  nfh.dwHeadSize = nhdgen_32le(sizeof(nfh)); /* header size */
  printf("Input the disk image comment. (255 letters max)\n");
  fgets(strbuf, 0x100, stdin);
  /* unlike win32, this will have a trailing newline on it we need to remove */
  strbuf[strcspn(strbuf, "\n")] = 0;
  strncpy(nfh.szComment, strbuf, sizeof(nfh.szComment));

  if(manual){
    manualHeader(&nfh);
  }else{
    autoHeader(srcfile, &nfh);
  }

  if(merge){
    mergeHeader(&nfh, srcfile, dstfile);
  }else{
    writeHeader(&nfh, dstfile);
  }

  return 0;
}
