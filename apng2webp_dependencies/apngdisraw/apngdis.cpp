/* APNG Disassembler 2.6
 *
 * Deconstructs APNG files into individual frames.
 *
 * http://apngdis.sourceforge.net
 *
 * Copyright (c) 2010-2012 Max Stepin
 * maxst at users.sourceforge.net
 *
 * zlib license
 * ------------
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <vector>
#include <cstring>
#include "png.h"     /* original (unpatched) libpng is ok */
#include "zlib.h"
#include "json/writer.h"
using namespace std;

#if defined(_MSC_VER) && _MSC_VER >= 1300
#define swap16(data) _byteswap_ushort(data)
#define swap32(data) _byteswap_ulong(data)
#elif defined(__linux__)
#include <byteswap.h>
#define swap16(data) bswap_16(data)
#define swap32(data) bswap_32(data)
#elif defined(__FreeBSD__)
#include <sys/endian.h>
#define swap16(data) bswap16(data)
#define swap32(data) bswap32(data)
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define swap16(data) OSSwapInt16(data)
#define swap32(data) OSSwapInt32(data)
#else
unsigned short swap16(unsigned short data) {return((data & 0xFF) << 8) | ((data >> 8) & 0xFF);}
unsigned int swap32(unsigned int data) {return((data & 0xFF) << 24) | ((data & 0xFF00) << 8) | ((data >> 8) & 0xFF00) | ((data >> 24) & 0xFF);}
#endif

#define notabc(c) ((c) < 65 || (c) > 122 || ((c) > 90 && (c) < 97))

#define id_IHDR 0x52444849
#define id_acTL 0x4C546361
#define id_fcTL 0x4C546366
#define id_IDAT 0x54414449
#define id_fdAT 0x54416466
#define id_IEND 0x444E4549

struct CHUNK { unsigned char * p; unsigned int size; };

#define APNG_DISPOSE_OP_NONE 0
#define APNG_DISPOSE_OP_BACKGROUND 1
#define APNG_DISPOSE_OP_PREVIOUS 2

#define APNG_BLEND_OP_SOURCE 0
#define APNG_BLEND_OP_OVER 1

struct APNGFrame
{
  unsigned char * p;
  unsigned int x, y;
  unsigned int w, h;
  unsigned int delay_num, delay_den;
  unsigned int blend_op, dispose_op;
  png_bytep * rows;
};

vector<CHUNK> info_chunks;
vector<APNGFrame> frames;

unsigned int num_frames = 1;

void info_fn(png_structp png_ptr, png_infop info_ptr)
{
  png_set_expand(png_ptr);
  png_set_strip_16(png_ptr);
  png_set_gray_to_rgb(png_ptr);
  png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
  (void)png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
}

void row_fn(png_structp png_ptr, png_bytep new_row, png_uint_32 row_num, int pass)
{
  APNGFrame * frame = (APNGFrame *)png_get_progressive_ptr(png_ptr);
  png_progressive_combine_row(png_ptr, frame->rows[row_num], new_row);
}

void compose_frame(unsigned char ** rows_dst, unsigned char ** rows_src, unsigned char bop, unsigned int w, unsigned int h)
{
  unsigned int  i, j;
  int u, v, al;

  for (j=0; j<h; j++)
  {
    unsigned char * sp = rows_src[j];
    unsigned char * dp = rows_dst[j];

    memset(dp, 0, w*4);

    if (bop == APNG_BLEND_OP_SOURCE)
      memcpy(dp, sp, w*4);
    else
    for (i=0; i<w; i++, sp+=4, dp+=4)
    {
      if (sp[3] == 255)
        memcpy(dp, sp, 4);
      else
      if (sp[3] != 0)
      {
        if (dp[3] != 0)
        {
          u = sp[3]*255;
          v = (255-sp[3])*dp[3];
          al = 255*255-(255-sp[3])*(255-dp[3]);
          dp[0] = (sp[0]*u + dp[0]*v)/al;
          dp[1] = (sp[1]*u + dp[1]*v)/al;
          dp[2] = (sp[2]*u + dp[2]*v)/al;
          dp[3] = al/255;
        }
        else
          memcpy(dp, sp, 4);
      }
    }
  }
}

unsigned int read_chunk(FILE * f, CHUNK * pChunk)
{
  unsigned int len;
  if (fread(&len, 4, 1, f) == 1)
  {
    pChunk->size = swap32(len) + 12;
    pChunk->p = new unsigned char[pChunk->size];
    unsigned int * pi = (unsigned int *)pChunk->p;
    pi[0] = len;
    if (fread(pChunk->p + 4, pChunk->size - 4, 1, f) == 1)
      return pi[1];
  }
  return 0;
}

void recalc_crc(unsigned char * p, unsigned int size)
{
  unsigned int crc = crc32(0, Z_NULL, 0);
  crc = crc32(crc, p + 4, size - 8);
  crc = swap32(crc);
  memcpy(p + size - 4, &crc, 4);
}

int LoadAPNG(char * szIn)
{
  FILE         * f;
  unsigned int   id, i, j, w, h, w0, h0, x0, y0;
  unsigned int   delay_num, delay_den, dop, bop, rowbytes, imagesize;
  unsigned int * pi;
  CHUNK          chunk_ihdr;
  CHUNK          chunk;
  png_structp    png_ptr;
  png_infop      info_ptr;
  unsigned char  sig[8];
  unsigned char  header[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  unsigned char  footer[12] = {0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130};
  unsigned int   flag_actl = 0;
  unsigned int   flag_fctl = 0;
  unsigned int   flag_idat = 0;
  unsigned int   flag_info = 0;
  APNGFrame      frameRaw = {0};
  APNGFrame      frameCur = {0};
  APNGFrame      frameNext = {0};
  int            res = 0;

  printf("Reading '%s'...\n", szIn);

  if ((f = fopen(szIn, "rb")) != 0)
  {
    if (fread(sig, 1, 8, f) == 8 && memcmp(sig, header, 8) == 0)
    {
      id = read_chunk(f, &chunk_ihdr);

      if (id == id_IHDR && chunk_ihdr.size == 25)
      {
        pi = (unsigned int *)chunk_ihdr.p;
        w0 = w = swap32(pi[2]);
        h0 = h = swap32(pi[3]);
        x0 = 0;
        y0 = 0;
        delay_num = 1;
        delay_den = 10;
        dop = APNG_DISPOSE_OP_NONE;
        bop = APNG_BLEND_OP_SOURCE;
        rowbytes = w * 4;
        imagesize = h * rowbytes;

        frameRaw.p = new unsigned char[imagesize];
        frameRaw.rows = new png_bytep[h * sizeof(png_bytep)];
        for (j=0; j<h; ++j)
          frameRaw.rows[j] = frameRaw.p + j * rowbytes;

        frameCur.w = w;
        frameCur.h = h;
        frameCur.p = new unsigned char[imagesize];
        frameCur.rows = new png_bytep[h * sizeof(png_bytep)];
        for (j=0; j<h; ++j)
          frameCur.rows[j] = frameCur.p + j * rowbytes;

        png_ptr  = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        info_ptr = png_create_info_struct(png_ptr);
        setjmp(png_jmpbuf(png_ptr));
        png_set_progressive_read_fn(png_ptr, (void *)&frameRaw, info_fn, row_fn, NULL);
        png_process_data(png_ptr, info_ptr, &header[0], 8);
        png_process_data(png_ptr, info_ptr, chunk_ihdr.p, chunk_ihdr.size);
        flag_info = 0;

        while ( !feof(f) )
        {
          id = read_chunk(f, &chunk);
          pi = (unsigned int *)chunk.p;

          if (id == id_acTL)
          {
            flag_actl = 1;
            num_frames = swap32(pi[2]);
            delete[] chunk.p;
          }
          else
          if (id == id_fcTL)
          {
            if (flag_fctl)
            {
              png_process_data(png_ptr, info_ptr, &footer[0], 12);
              png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

              frameNext.p = new unsigned char[imagesize];
              frameNext.rows = new png_bytep[h * sizeof(png_bytep)];
              for (j=0; j<h; ++j)
                frameNext.rows[j] = frameNext.p + j * rowbytes;

              if (dop == APNG_DISPOSE_OP_PREVIOUS)
                memcpy(frameNext.p, frameCur.p, imagesize);

              compose_frame(frameCur.rows, frameRaw.rows, bop, w0, h0);
              frameCur.blend_op = bop;
              frameCur.dispose_op = dop;
              frameCur.x = x0;
              frameCur.y = y0;
              frameCur.w = w0;
              frameCur.h = h0;
              frameCur.delay_num = delay_num;
              frameCur.delay_den = delay_den;
              frames.push_back(frameCur);

              if (dop != APNG_DISPOSE_OP_PREVIOUS)
              {
                memcpy(frameNext.p, frameCur.p, imagesize);
                if (dop == APNG_DISPOSE_OP_BACKGROUND)
                  for (j=0; j<h0; j++)
                    memset(frameNext.rows[y0 + j] + x0*4, 0, w0*4);
              }
              frameCur.p = frameNext.p;
              frameCur.rows = frameNext.rows;

              memcpy(chunk_ihdr.p + 8, chunk.p + 12, 8);
              recalc_crc(chunk_ihdr.p, chunk_ihdr.size);

              png_ptr  = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
              info_ptr = png_create_info_struct(png_ptr);
              setjmp(png_jmpbuf(png_ptr));
              png_set_progressive_read_fn(png_ptr, (void *)&frameRaw, info_fn, row_fn, NULL);
              png_process_data(png_ptr, info_ptr, &header[0], 8);
              png_process_data(png_ptr, info_ptr, chunk_ihdr.p, chunk_ihdr.size);
              flag_info = 0;
            }

            w0 = swap32(pi[3]);
            h0 = swap32(pi[4]);
            x0 = swap32(pi[5]);
            y0 = swap32(pi[6]);
            delay_num = chunk.p[28]*256 + chunk.p[29];
            delay_den = chunk.p[30]*256 + chunk.p[31];
            dop = chunk.p[32]; // dispose_op
            bop = chunk.p[33]; // blend_op
            if (!flag_fctl)
            {
              bop = APNG_BLEND_OP_SOURCE;
              if (dop == APNG_DISPOSE_OP_PREVIOUS)
                dop = APNG_DISPOSE_OP_BACKGROUND;
            }
            flag_fctl = 1;
            delete[] chunk.p;
          }
          else
          if (id == id_IDAT)
          {
            flag_idat = 1;
            if (flag_fctl || !flag_actl)
            {
              if (!flag_info)
              {
                flag_info = 1;
                for (i=0; i<info_chunks.size(); ++i)
                  png_process_data(png_ptr, info_ptr, info_chunks[i].p, info_chunks[i].size);
              }
              png_process_data(png_ptr, info_ptr, chunk.p, chunk.size);
            }
            delete[] chunk.p;
          }
          else
          if (id == id_fdAT)
          {
            flag_idat = 1;
            if (!flag_info)
            {
              flag_info = 1;
              for (i=0; i<info_chunks.size(); ++i)
                png_process_data(png_ptr, info_ptr, info_chunks[i].p, info_chunks[i].size);
            }
            pi[1] = swap32(chunk.size - 16);
            pi[2] = id_IDAT;
            recalc_crc(chunk.p + 4, chunk.size - 4);
            png_process_data(png_ptr, info_ptr, chunk.p + 4, chunk.size - 4);
            delete[] chunk.p;
          }
          else
          if (id == id_IEND)
          {
            png_process_data(png_ptr, info_ptr, &footer[0], 12);

            compose_frame(frameCur.rows, frameRaw.rows, bop, w0, h0);
            frameCur.blend_op = bop;
            frameCur.dispose_op = dop;
            frameCur.x = x0;
            frameCur.y = y0;
            frameCur.w = w0;
            frameCur.h = h0;
            frameCur.delay_num = delay_num;
            frameCur.delay_den = delay_den;
            frames.push_back(frameCur);
            delete[] chunk.p;
            break;
          }
          else
          if (notabc(chunk.p[4]) || notabc(chunk.p[5]) || notabc(chunk.p[6]) || notabc(chunk.p[7]))
          {
            delete[] chunk.p;
            break;
          }
          else
          {
            if (!flag_idat)
              info_chunks.push_back(chunk);
            else
              delete[] chunk.p;
          }
        }
        delete[] frameRaw.rows;
        delete[] frameRaw.p;
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      }
      else
        res = 1;
    }
    else
      res = 1;

    fclose(f);

    for (i=0; i<info_chunks.size(); ++i)
      delete[] info_chunks[i].p;

    info_chunks.clear();
    delete[] chunk_ihdr.p;
  }
  else
    res = 1;

  return res;
}

void SavePNG(char * szOut, APNGFrame * frame)
{
  FILE * f;
  if ((f = fopen(szOut, "wb")) != 0)
  {
    png_structp  png_ptr  = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop    info_ptr = png_create_info_struct(png_ptr);
    if (png_ptr != NULL && info_ptr != NULL && setjmp(png_jmpbuf(png_ptr)) == 0)
    {
      png_init_io(png_ptr, f);
      png_set_compression_level(png_ptr, 9);
      png_set_IHDR(png_ptr, info_ptr, frame->w, frame->h, 8, 6, 0, 0, 0);
      png_write_info(png_ptr, info_ptr);
      png_write_image(png_ptr, frame->rows);
      png_write_end(png_ptr, info_ptr);
    }
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(f);
  }
}

void SaveTXT(char * szOut, APNGFrame * frame)
{
  FILE * f;
  if ((f = fopen(szOut, "wb")) != 0)
  {
    fprintf(f, "delay=%d/%d\n", frame->delay_num, frame->delay_den);
    fclose(f);
  }
}

int main(int argc, char** argv)
{
  unsigned int i, j, len;
  char * szInput;
  char * szOutPrefix;
  char   szPath[256];
  const char * szFilename;
  char   szOut[256];
  Json::Value apng_obj;
  Json::Value frames_vec(Json::arrayValue);

  printf("\nAPNG Disassembler 2.6\n\n");

  if (argc > 1)
    szInput = argv[1];
  else
  {
    printf("Usage: apngdis anim.png [name]\n");
    return 1;
  }
  strcpy(szPath, szInput);
  for (i=j=0; szPath[i]!=0; i++)
  {
    if (szPath[i] == '\\' || szPath[i] == '/' || szPath[i] == ':')
      j = i+1;
  }
  szPath[j] = 0;

  if (argc > 2)
  {
    szOutPrefix = argv[2];

    for (i=j=0; szOutPrefix[i]!=0; i++)
    {
      if (szOutPrefix[i] == '\\' || szOutPrefix[i] == '/' || szOutPrefix[i] == ':')
        j = i+1;
      if (szOutPrefix[i] == '.')
        szOutPrefix[i] = 0;
    }
    strcat(szPath, szOutPrefix+j);
    szFilename = szOutPrefix+j;
  }
  else
  {
    szFilename = "apngframe";
    strcat(szPath, "apngframe");
  }

  if (LoadAPNG(szInput) != 0)
  {
    printf("LoadAPNG() failed: '%s'\n ", szInput);
    return 1;
  }

  len = sprintf(szOut, "%d", num_frames);
  for (i=0; i<frames.size(); ++i)
  {
    printf("extracting frame %d of %d\n", i+1, num_frames);

    sprintf(szOut, "%s%.*d.png", szPath, len, i+1);
    SavePNG(szOut, &frames[i]);

    Json::Value frame_metadata;
    sprintf(szOut, "%s%.*d.png", szFilename, len, i+1);
    frame_metadata["src"] = Json::Value(szOut);
    frame_metadata["delay_num"] = Json::Value(frames[i].delay_num);
    frame_metadata["delay_den"] = Json::Value(frames[i].delay_den);
    frame_metadata["x"] = Json::Value(frames[i].x);
    frame_metadata["y"] = Json::Value(frames[i].y);
    frame_metadata["blend_op"] = Json::Value(frames[i].blend_op);
    frame_metadata["dispose_op"] = Json::Value(frames[i].dispose_op);
    frames_vec.append(frame_metadata);

    delete[] frames[i].rows;
    delete[] frames[i].p;
  }
  frames.clear();
  apng_obj["frames"] = frames_vec;

  Json::StyledWriter writer;
  std::string frames_metadata = writer.write( apng_obj );
  cout << frames_metadata << endl;
  ofstream metadata_f;
  sprintf(szOut, "%s_metadata.json", szPath);
  metadata_f.open (szOut);
  metadata_f << frames_metadata;
  metadata_f.close();

  printf("all done\n");

  return 0;
}
