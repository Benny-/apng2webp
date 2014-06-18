/* APNG Optimizer 1.0
 * http://sourceforge.net/projects/apng/files
 *
 * Copyright (c) 2011 Max Stepin
 * maxst at users.sourceforge.net
 *
 * GNU LGPL information
 * --------------------
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "zlib.h"

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

#define PNG_ZBUF_SIZE  32768

#define PNG_DISPOSE_OP_NONE        0x00
#define PNG_DISPOSE_OP_BACKGROUND  0x01
#define PNG_DISPOSE_OP_PREVIOUS    0x02

#define PNG_BLEND_OP_SOURCE        0x00
#define PNG_BLEND_OP_OVER          0x01

#define notabc(c) ((c) < 65 || (c) > 122 || ((c) > 90 && (c) < 97))

#define ROWBYTES(pixel_bits, width) \
    ((pixel_bits) >= 8 ? \
    ((width) * (((unsigned int)(pixel_bits)) >> 3)) : \
    (( ((width) * ((unsigned int)(pixel_bits))) + 7) >> 3) )

typedef struct { z_stream zstream; unsigned char * zbuf; int x, y, w, h, valid; } OP;
typedef struct { unsigned int i, num, trans; } STATS;

OP    op[12];
STATS stats[256];

unsigned int    next_seq_num = 0;
unsigned char * row_buf;
unsigned char * sub_row;
unsigned char * up_row;
unsigned char * avg_row;
unsigned char * paeth_row;
unsigned char   png_sign[8] = {137, 80, 78, 71, 13, 10, 26, 10};
int             mask4[2]={240,15};
int             shift4[2]={4,0};
int             mask2[4]={192,48,12,3};
int             shift2[4]={6,4,2,0};
int             mask1[8]={128,64,32,16,8,4,2,1};
int             shift1[8]={7,6,5,4,3,2,1,0};
unsigned int    keep_original = 1;
unsigned char   pal[256][3];
unsigned char   trns[256];
unsigned int    palsize, trnssize;
unsigned short  trns1, trns2, trns3;
int             trns_idx;
int             keep_palette = 0;
int             keep_coltype = 0;

int cmp_stats( const void *arg1, const void *arg2 )
{
  if ( ((STATS*)arg2)->trans == ((STATS*)arg1)->trans )
    return ((STATS*)arg2)->num - ((STATS*)arg1)->num;
  else
    return ((STATS*)arg2)->trans - ((STATS*)arg1)->trans;
}

int read32(unsigned int *val, FILE * f1)
{
  unsigned char a, b, c, d;
  if (fread(&a, 1, 1, f1) == 1 && fread(&b, 1, 1, f1) == 1 && fread(&c, 1, 1, f1) == 1 && fread(&d, 1, 1, f1) == 1)
  {
    *val = ((unsigned int)a<<24)+((unsigned int)b<<16)+((unsigned int)c<<8)+(unsigned int)d;
    return 0;
  }
  return 1;
}

int read16(unsigned short *val, FILE * f1)
{
  unsigned char a, b;
  if (fread(&a, 1, 1, f1) == 1 && fread(&b, 1, 1, f1) == 1)
  {
    *val = ((unsigned short)a<<8)+(unsigned short)b;
    return 0;
  }
  return 1;
}

unsigned short readshort(unsigned char * p)
{
  return ((unsigned short)(*p)<<8)+(unsigned short)(*(p+1));
}

void read_sub_row(unsigned char * row, unsigned int rowbytes, unsigned int bpp)
{
  unsigned int i;

  for (i=bpp; i<rowbytes; i++)
    row[i] += row[i-bpp];
}

void read_up_row(unsigned char * row, unsigned char * prev_row, unsigned int rowbytes, unsigned int bpp)
{
  unsigned int i;

  if (prev_row)
    for (i=0; i<rowbytes; i++)
      row[i] += prev_row[i];
}

void read_average_row(unsigned char * row, unsigned char * prev_row, unsigned int rowbytes, unsigned int bpp)
{
  unsigned int i;

  if (prev_row)
  {
    for (i=0; i<bpp; i++)
      row[i] += prev_row[i]>>1;
    for (i=bpp; i<rowbytes; i++)
      row[i] += (prev_row[i] + row[i-bpp])>>1;
  } 
  else 
  {
    for (i=bpp; i<rowbytes; i++)
      row[i] += row[i-bpp]>>1;
  }
}

void read_paeth_row(unsigned char * row, unsigned char * prev_row, unsigned int rowbytes, unsigned int bpp)
{
  unsigned int i;
  int a, b, c, pa, pb, pc, p;

  if (prev_row) 
  {
    for (i=0; i<bpp; i++)
      row[i] += prev_row[i];
    for (i=bpp; i<rowbytes; i++)
    {
      a = row[i-bpp];
      b = prev_row[i];
      c = prev_row[i-bpp];
      p = b - c;
      pc = a - c;
      pa = abs(p);
      pb = abs(pc);
      pc = abs(p + pc);
      row[i] += ((pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c);
    }
  } 
  else 
  {
    for (i=bpp; i<rowbytes; i++)
      row[i] += row[i-bpp];
  }
}

void unpack(z_stream zstream, unsigned char * dst, unsigned int dst_size, unsigned char * src, unsigned int src_size, unsigned int h, unsigned int rowbytes, unsigned char bpp)
{
  unsigned int    j;
  unsigned char * row = dst;
  unsigned char * prev_row = NULL;

  zstream.next_out  = dst;
  zstream.avail_out = dst_size;
  zstream.next_in   = src;
  zstream.avail_in  = src_size;
  inflate(&zstream, Z_FINISH);
  inflateReset(&zstream);

  for (j=0; j<h; j++)
  {
    switch (*row++) 
    {
      case 0: break;
      case 1: read_sub_row(row, rowbytes, bpp); break;
      case 2: read_up_row(row, prev_row, rowbytes, bpp); break;
      case 3: read_average_row(row, prev_row, rowbytes, bpp); break;
      case 4: read_paeth_row(row, prev_row, rowbytes, bpp); break;
    }
    prev_row = row;
    row += rowbytes;
  }
}

void compose0(unsigned char * dst1, unsigned int dstbytes1, unsigned char * dst2, unsigned int dstbytes2, unsigned char * src, unsigned int srcbytes, unsigned int w, unsigned int h, unsigned int bop, unsigned char depth)
{
  unsigned int    i, j, g, a;
  unsigned char * sp;
  unsigned char * dp1;
  unsigned int  * dp2;

  for (j=0; j<h; j++)
  {
    sp = src+1;
    dp1 = dst1;
    dp2 = (unsigned int*)dst2;

    if (bop == PNG_BLEND_OP_SOURCE)
    {
      switch (depth)
      {
        case 16: for (i=0; i<w; i++) { a = 0xFF; if (trnssize && readshort(sp)==trns1) a = 0; *dp1++ = *sp; *dp2++ = (a << 24) + (*sp << 16) + (*sp << 8) + *sp; sp+=2; }  break;
        case 8:  for (i=0; i<w; i++) { a = 0xFF; if (trnssize && *sp==trns1)           a = 0; *dp1++ = *sp; *dp2++ = (a << 24) + (*sp << 16) + (*sp << 8) + *sp; sp++;  }  break;
        case 4:  for (i=0; i<w; i++) { g = (sp[i>>1] & mask4[i&1]) >> shift4[i&1]; a = 0xFF; if (trnssize && g==trns1) a = 0; *dp1++ = g*0x11; *dp2++ = (a<<24) + g*0x111111; } break;
        case 2:  for (i=0; i<w; i++) { g = (sp[i>>2] & mask2[i&3]) >> shift2[i&3]; a = 0xFF; if (trnssize && g==trns1) a = 0; *dp1++ = g*0x55; *dp2++ = (a<<24) + g*0x555555; } break;
        case 1:  for (i=0; i<w; i++) { g = (sp[i>>3] & mask1[i&7]) >> shift1[i&7]; a = 0xFF; if (trnssize && g==trns1) a = 0; *dp1++ = g*0xFF; *dp2++ = (a<<24) + g*0xFFFFFF; } break;
      }
    }
    else /* PNG_BLEND_OP_OVER */
    {
      switch (depth)
      {
        case 16: for (i=0; i<w; i++, dp1++, dp2++) { if (readshort(sp) != trns1) { *dp1 = *sp; *dp2 = 0xFF000000 + (*sp << 16) + (*sp << 8) + *sp; } sp+=2; } break;
        case 8:  for (i=0; i<w; i++, dp1++, dp2++) { if (*sp != trns1)           { *dp1 = *sp; *dp2 = 0xFF000000 + (*sp << 16) + (*sp << 8) + *sp; } sp++;  } break;
        case 4:  for (i=0; i<w; i++, dp1++, dp2++) { g = (sp[i>>1] & mask4[i&1]) >> shift4[i&1]; if (g != trns1) { *dp1 = g*0x11; *dp2 = 0xFF000000+g*0x111111; } } break;
        case 2:  for (i=0; i<w; i++, dp1++, dp2++) { g = (sp[i>>2] & mask2[i&3]) >> shift2[i&3]; if (g != trns1) { *dp1 = g*0x55; *dp2 = 0xFF000000+g*0x555555; } } break;
        case 1:  for (i=0; i<w; i++, dp1++, dp2++) { g = (sp[i>>3] & mask1[i&7]) >> shift1[i&7]; if (g != trns1) { *dp1 = g*0xFF; *dp2 = 0xFF000000+g*0xFFFFFF; } } break;
      }
    }

    src += srcbytes;
    dst1 += dstbytes1;
    dst2 += dstbytes2;
  }
}

void compose2(unsigned char * dst1, unsigned int dstbytes1, unsigned char * dst2, unsigned int dstbytes2, unsigned char * src, unsigned int srcbytes, unsigned int w, unsigned int h, unsigned int bop, unsigned char depth)
{
  unsigned int    i, j;
  unsigned int    r, g, b, a;
  unsigned char * sp;
  unsigned char * dp1;
  unsigned int  * dp2;

  for (j=0; j<h; j++)
  {
    sp = src+1;
    dp1 = dst1;
    dp2 = (unsigned int*)dst2;

    if (bop == PNG_BLEND_OP_SOURCE)
    {
      if (depth == 8)
      {
        for (i=0; i<w; i++)
        {
          r = *sp++;
          g = *sp++;
          b = *sp++;
          a = 0xFF;
          if (trnssize && r==trns1 && g==trns2 && b==trns3)
            a = 0;
          *dp1++ = r; *dp1++ = g; *dp1++ = b;
          *dp2++ = (a << 24) + (b << 16) + (g << 8) + r;
        }
      }
      else
      {
        for (i=0; i<w; i++, sp+=6)
        {
          r = *sp;
          g = *(sp+2);
          b = *(sp+4);
          a = 0xFF;
          if (trnssize && readshort(sp)==trns1 && readshort(sp+2)==trns2 && readshort(sp+4)==trns3)
            a = 0;
          *dp1++ = r; *dp1++ = g; *dp1++ = b;
          *dp2++ = (a << 24) + (b << 16) + (g << 8) + r;
        }
      }
    }
    else /* PNG_BLEND_OP_OVER */
    {
      if (depth == 8)
      {
        for (i=0; i<w; i++, sp+=3, dp1+=3, dp2++)
        if ((*sp != trns1) || (*(sp+1) != trns2) || (*(sp+2) != trns3))
        {
          *dp1 = *sp; *(dp1+1) = *(sp+1); *(dp1+2) = *(sp+2);
          *dp2 = 0xFF000000 + (*(sp+2) << 16) + (*(sp+1) << 8) + *sp;
        }
      }
      else
      {
        for (i=0; i<w; i++, sp+=6, dp1+=3, dp2++)
        if ((readshort(sp) != trns1) || (readshort(sp+2) != trns2) || (readshort(sp+4) != trns3))
        {
          *dp1 = *sp; *(dp1+1) = *(sp+2); *(dp1+2) = *(sp+4);
          *dp2 = 0xFF000000 + (*(sp+4) << 16) + (*(sp+2) << 8) + *sp;
        }
      }
    }
    src += srcbytes;
    dst1 += dstbytes1;
    dst2 += dstbytes2;
  }
}

void compose3(unsigned char * dst1, unsigned int dstbytes1, unsigned char * dst2, unsigned int dstbytes2, unsigned char * src, unsigned int srcbytes, unsigned int w, unsigned int h, unsigned int bop, unsigned char depth)
{
  unsigned int    i, j;
  unsigned int    r, g, b, a;
  unsigned int    r2, g2, b2, a2;
  int             u, v, al;
  unsigned char   col;
  unsigned char * sp;
  unsigned char * dp1;
  unsigned int  * dp2;

  for (j=0; j<h; j++)
  {
    sp = src+1;
    dp1 = dst1;
    dp2 = (unsigned int*)dst2;

    for (i=0; i<w; i++)
    {
      col = sp[i];

      switch (depth)
      {
        case 4: col = (sp[i>>1] & mask4[i&1]) >> shift4[i&1]; break;
        case 2: col = (sp[i>>2] & mask2[i&3]) >> shift2[i&3]; break;
        case 1: col = (sp[i>>3] & mask1[i&7]) >> shift1[i&7]; break;
      }

      r = pal[col][0];
      g = pal[col][1];
      b = pal[col][2];
      a = trns[col];

      if (bop == PNG_BLEND_OP_SOURCE)
      {
        *dp1++ = col;
        *dp2++ = (a << 24) + (b << 16) + (g << 8) + r;
      }
      else /* PNG_BLEND_OP_OVER */
      {
        if (a == 255)
        {
          *dp1++ = col;
          *dp2++ = (a << 24) + (b << 16) + (g << 8) + r;
        }
        else
        if (a != 0)
        {
          if ((a2 = (*dp2)>>24) != 0)
          {
            keep_original = 0;
            u = a*255;
            v = (255-a)*a2;
            al = 255*255-(255-a)*(255-a2);
            r2 = ((*dp2)&255);
            g2 = (((*dp2)>>8)&255);
            b2 = (((*dp2)>>16)&255);
            r = (r*u + r2*v)/al;
            g = (g*u + g2*v)/al;
            b = (b*u + b2*v)/al;
            a = al/255;
          }
          *dp1++ = col;
          *dp2++ = (a << 24) + (b << 16) + (g << 8) + r;
        }
        else
        {
          dp1++;
          dp2++;
        }
      }
    }
    src += srcbytes;
    dst1 += dstbytes1;
    dst2 += dstbytes2;
  }
}

void compose4(unsigned char * dst, unsigned int dstbytes, unsigned char * src, unsigned int srcbytes, unsigned int w, unsigned int h, unsigned int bop, unsigned char depth)
{
  unsigned int    i, j, step;
  unsigned int    g, a, g2, a2;
  int             u, v, al;
  unsigned char * sp;
  unsigned char * dp;

  step = (depth+7)/8;

  for (j=0; j<h; j++)
  {
    sp = src+1;
    dp = dst;

    if (bop == PNG_BLEND_OP_SOURCE)
    {
      for (i=0; i<w; i++)
      {
        g = *sp; sp += step;
        a = *sp; sp += step;
        *dp++ = g;
        *dp++ = a;
      }
    }
    else /* PNG_BLEND_OP_OVER */
    {
      for (i=0; i<w; i++)
      {
        g = *sp; sp += step;
        a = *sp; sp += step;
        if (a == 255)
        {
          *dp++ = g;
          *dp++ = a;
        }
        else
        if (a != 0)
        {
          if ((a2 = *(dp+1)) != 0)
          {
            u = a*255;
            v = (255-a)*a2;
            al = 255*255-(255-a)*(255-a2);
            g2 = ((*dp)&255);
            g = (g*u + g2*v)/al;
            a = al/255;
          }
          *dp++ = g;
          *dp++ = a;
        }
        else
          dp+=2;
      }
    }
    src += srcbytes;
    dst += dstbytes;
  }
}

void compose6(unsigned char * dst, unsigned int dstbytes, unsigned char * src, unsigned int srcbytes, unsigned int w, unsigned int h, unsigned int bop, unsigned char depth)
{
  unsigned int    i, j, step;
  unsigned int    r, g, b, a;
  unsigned int    r2, g2, b2, a2;
  int             u, v, al;
  unsigned char * sp;
  unsigned int  * dp;

  step = (depth+7)/8;

  for (j=0; j<h; j++)
  {
    sp = src+1;
    dp = (unsigned int*)dst;

    if (bop == PNG_BLEND_OP_SOURCE)
    {
      for (i=0; i<w; i++)
      {
        r = *sp; sp += step;
        g = *sp; sp += step;
        b = *sp; sp += step;
        a = *sp; sp += step;
        *dp++ = (a << 24) + (b << 16) + (g << 8) + r;
      }
    }
    else /* PNG_BLEND_OP_OVER */
    {
      for (i=0; i<w; i++)
      {
        r = *sp; sp += step;
        g = *sp; sp += step;
        b = *sp; sp += step;
        a = *sp; sp += step;
        if (a == 255)
          *dp++ = (a << 24) + (b << 16) + (g << 8) + r;
        else
        if (a != 0)
        {
          if ((a2 = (*dp)>>24) != 0)
          {
            u = a*255;
            v = (255-a)*a2;
            al = 255*255-(255-a)*(255-a2);
            r2 = ((*dp)&255);
            g2 = (((*dp)>>8)&255);
            b2 = (((*dp)>>16)&255);
            r = (r*u + r2*v)/al;
            g = (g*u + g2*v)/al;
            b = (b*u + b2*v)/al;
            a = al/255;
          }
          *dp++ = (a << 24) + (b << 16) + (g << 8) + r;
        }
        else
          dp++;
      }
    }
    src += srcbytes;
    dst += dstbytes;
  }
}

int LoadAPNG(char * szIn, unsigned int *pWidth, unsigned int *pHeight, unsigned char *pColType, unsigned int *pFirst, unsigned int *pFrames, unsigned int *pLoops, unsigned char **ppOut1, unsigned char **ppOut2, unsigned short **ppDelays)
{
  unsigned char     sig[8];
  unsigned int      i, j;
  unsigned int      len, chunk, seq, crc;
  unsigned int      w, h, w0, h0, x0, y0;
  unsigned char     depth, coltype, compr, filter, interl;
  unsigned char     pixeldepth, bpp;
  unsigned int      rowbytes, frames, loops, first_visible, cur_frame, channels;
  unsigned int      outrow1, outrow2, outimg1, outimg2;
  unsigned short    d1, d2;
  unsigned char     c, dop, bop;
  int               imagesize, zbuf_size, zsize;
  z_stream          zstream;
  unsigned char   * pOut1;
  unsigned char   * pOut2;
  unsigned char   * pTemp;
  unsigned char   * pData;
  unsigned char   * pImg1;
  unsigned char   * pImg2;
  unsigned char   * pDst1;
  unsigned char   * pDst2;
  unsigned short  * pDelays;
  FILE            * f1;
  int               res = 1;

  if ((f1 = fopen(szIn, "rb")) == 0)
  {
    printf("Error: can't open '%s'\n", szIn);
    return res;
  }

  printf("Reading '%s'...\n", szIn);

  frames = 0;
  loops = 0;
  first_visible = 0;
  cur_frame = 0;
  zsize = 0;
  x0 = 0;
  y0 = 0;
  bop = PNG_BLEND_OP_SOURCE;
  palsize = 0;
  trnssize = 0;
  trns_idx = -1;

  memset(trns, 255, 256);

  zstream.zalloc = Z_NULL;
  zstream.zfree = Z_NULL;
  zstream.opaque = Z_NULL;
  inflateInit(&zstream);

  do
  {
    if (fread(sig, 1, 8, f1) != 8)
    {
      printf("Error: can't read the sig\n");
      break;
    }
    if (memcmp(sig, png_sign, 8) != 0) 
    {
      printf("Error: wrong PNG sig\n");
      break;
    }

    if (read32(&len, f1)) break;
    if (read32(&chunk, f1)) break;

    if (len != 13 || chunk != 0x49484452) /* IHDR */
    {
      printf("Error: missing IHDR\n");
      break;
    }

    if (read32(&w, f1)) break;
    if (read32(&h, f1)) break;
    w0 = w;
    h0 = h;
    if (fread(&depth, 1, 1, f1) != 1) break;
    if (fread(&coltype, 1, 1, f1) != 1) break;
    if (fread(&compr, 1, 1, f1) != 1) break;
    if (fread(&filter, 1, 1, f1) != 1) break;
    if (fread(&interl, 1, 1, f1) != 1) break;
    if (read32(&crc, f1)) break;

    channels = 1;
    if (coltype == 2)
      channels = 3;
    else
    if (coltype == 4)
      channels = 2;
    else
    if (coltype == 6)
      channels = 4;

    pixeldepth = depth*channels;
    bpp = (pixeldepth + 7) >> 3;
    rowbytes = ROWBYTES(pixeldepth, w);

    imagesize = (rowbytes + 1) * h;
    zbuf_size = imagesize + ((imagesize + 7) >> 3) + ((imagesize + 63) >> 6) + 11;

    /*
     * We'll render into 2 output buffers, first in original coltype,
     * second in RGBA.
     *
     * It's better to try to keep the original coltype, but if dispose/blend
     * operations will make it impossible, then we'll save RGBA version instead.
     */

    outrow1 = w*channels; /* output coltype = input coltype */
    outrow2 = w*4;        /* output coltype = RGBA          */
    outimg1 = h*outrow1;
    outimg2 = h*outrow2;

    pOut1   = (unsigned char *)malloc((frames+1)*outimg1);
    pOut2   = (unsigned char *)malloc((frames+1)*outimg2);
    pDelays = (unsigned short *)malloc((frames+1)*4);
    pTemp   = (unsigned char *)malloc(imagesize);
    pData   = (unsigned char *)malloc(zbuf_size);
    if (!pOut1 || !pOut2 || !pTemp || !pData)
    {
      printf("Error: not enough memory\n");
      break;
    }
    pImg1 = pOut1;
    pImg2 = pOut2;
    memset(pOut1, 0, outimg1);
    memset(pOut2, 0, outimg2);
    pDelays[0] = 0;
    pDelays[1] = 0;

    while ( !feof(f1) )
    {
      if (read32(&len, f1)) break;
      if (read32(&chunk, f1)) break;

      if (chunk == 0x504C5445) /* PLTE */
      {
        unsigned int col;
        for (i=0; i<len; i++)
        {
          if (fread(&c, 1, 1, f1) != 1) break;
          col = i/3;
          if (col<256)
          {
            pal[col][i%3] = c;
            palsize = col+1;
          }
        }
        if (read32(&crc, f1)) break;
      }
      else
      if (chunk == 0x74524E53) /* tRNS */
      {
        for (i=0; i<len; i++)
        {
          if (fread(&c, 1, 1, f1) != 1) break;
          if (i<256)
          {
            trns[i] = c;
            trnssize = i+1;
            if (c == 0 && coltype == 3 && trns_idx == -1) 
              trns_idx = i;
          }
        }
        if (coltype == 0)
        {
          if (trnssize == 2)
          {
            trns1 = readshort(&trns[0]);
            switch (depth)
            {
              case 16: trns[1] = trns[0]; trns[0] = 0; break;
              case 4: trns[1] *= 0x11; break;
              case 2: trns[1] *= 0x55; break;
              case 1: trns[1] *= 0xFF; break;
            }
            trns_idx = trns[1];
          }
          else
            trnssize = 0;
        }
        else
        if (coltype == 2)
        {
          if (trnssize == 6)
          {
            trns1 = readshort(&trns[0]);
            trns2 = readshort(&trns[2]);
            trns3 = readshort(&trns[4]);
            if (depth == 16)
            {
              trns[1] = trns[0]; trns[0] = 0;
              trns[3] = trns[2]; trns[2] = 0;
              trns[5] = trns[4]; trns[4] = 0;
            }
          }
          else
            trnssize = 0;
        }
        if (read32(&crc, f1)) break;
      }
      else
      if (chunk == 0x6163544C) /* acTL */
      {
        if (read32(&frames, f1)) break;
        if (read32(&loops, f1)) break;
        if (read32(&crc, f1)) break;
        free(pOut1);
        free(pOut2);
        free(pDelays);
        pOut1   = (unsigned char *)malloc((frames+1)*outimg1);
        pOut2   = (unsigned char *)malloc((frames+1)*outimg2);
        pDelays = (unsigned short *)malloc((frames+1)*4);
        pImg1   = pOut1;
        pImg2   = pOut2;
        memset(pOut1, 0, outimg1);
        memset(pOut2, 0, outimg2);
      }
      else
      if (chunk == 0x6663544C) /* fcTL */
      {
        if (zsize == 0)
          first_visible = 1;
        else
        {
          if (dop == PNG_DISPOSE_OP_PREVIOUS)
          {
            if (coltype != 6)
              memcpy(pImg1 + outimg1, pImg1, outimg1);
            if (coltype != 4)
              memcpy(pImg2 + outimg2, pImg2, outimg2);
          }

          pDst1 = pImg1 + y0*outrow1 + x0*channels;
          pDst2 = pImg2 + y0*outrow2 + x0*4;
          unpack(zstream, pTemp, imagesize, pData, zsize, h0, rowbytes, bpp);
          switch (coltype)
          {
            case 0: compose0(pDst1, outrow1, pDst2, outrow2, pTemp, rowbytes+1, w0, h0, bop, depth); break;
            case 2: compose2(pDst1, outrow1, pDst2, outrow2, pTemp, rowbytes+1, w0, h0, bop, depth); break;
            case 3: compose3(pDst1, outrow1, pDst2, outrow2, pTemp, rowbytes+1, w0, h0, bop, depth); break;
            case 4: compose4(pDst1, outrow1,                 pTemp, rowbytes+1, w0, h0, bop, depth); break;
            case 6: compose6(                pDst2, outrow2, pTemp, rowbytes+1, w0, h0, bop, depth); break;
          }
          zsize = 0;

          if (dop != PNG_DISPOSE_OP_PREVIOUS)
          {
            if (coltype != 6)
              memcpy(pImg1 + outimg1, pImg1, outimg1);
            if (coltype != 4)
              memcpy(pImg2 + outimg2, pImg2, outimg2);

            if (dop == PNG_DISPOSE_OP_BACKGROUND)
            {
              pDst1 += outimg1;
              pDst2 += outimg2;

              for (j=0; j<h0; j++)
              {
                switch (coltype)
                {
                  case 0:  memset(pDst2, 0, w0*4); if (trnssize) memset(pDst1, trns_idx, w0); else keep_original = 0; break;
                  case 2:  memset(pDst2, 0, w0*4); if (trnssize) for (i=0; i<w0; i++) { pDst1[i*3] = trns[1]; pDst1[i*3+1] = trns[3]; pDst1[i*3+2] = trns[5]; } else keep_original = 0; break;
                  case 3:  memset(pDst2, 0, w0*4); if (trns_idx >= 0) memset(pDst1, trns_idx, w0); else keep_original = 0; break;
                  case 4:  memset(pDst1, 0, w0*2); break;
                  case 6:  memset(pDst2, 0, w0*4); break;
                }
                pDst1 += outrow1;
                pDst2 += outrow2;
              }
            }
          }
        }

        if (read32(&seq, f1)) break;
        if (read32(&w0, f1)) break;
        if (read32(&h0, f1)) break;
        if (read32(&x0, f1)) break;
        if (read32(&y0, f1)) break;
        if (read16(&d1, f1)) break;
        if (read16(&d2, f1)) break;
        if (fread(&dop, 1, 1, f1) != 1) break;
        if (fread(&bop, 1, 1, f1) != 1) break;
        if (read32(&crc, f1)) break;

        if (cur_frame == 0)
        {
          bop = PNG_BLEND_OP_SOURCE;
          if (dop == PNG_DISPOSE_OP_PREVIOUS)
            dop = PNG_DISPOSE_OP_BACKGROUND;
        }

        if (coltype<=3 && trnssize==0)
          bop = PNG_BLEND_OP_SOURCE;

        rowbytes = ROWBYTES(pixeldepth, w0);
        cur_frame++;
        pImg1 += outimg1;
        pImg2 += outimg2;
        pDelays[cur_frame*2]   = d1;
        pDelays[cur_frame*2+1] = d2;
      }
      else
      if (chunk == 0x49444154) /* IDAT */
      {
        if (fread(pData + zsize, 1, len, f1) != len) break;
        zsize += len;
        if (read32(&crc, f1)) break;
      }
      else
      if (chunk == 0x66644154) /* fdAT */
      {
        if (read32(&seq, f1)) break;
        len -= 4;
        if (fread(pData + zsize, 1, len, f1) != len) break;
        zsize += len;
        if (read32(&crc, f1)) break;
      }
      else
      if (chunk == 0x49454E44) /* IEND */
      {
        pDst1 = pImg1 + y0*outrow1 + x0*channels;
        pDst2 = pImg2 + y0*outrow2 + x0*4;
        unpack(zstream, pTemp, imagesize, pData, zsize, h0, rowbytes, bpp);
        switch (coltype)
        {
          case 0: compose0(pDst1, outrow1, pDst2, outrow2, pTemp, rowbytes+1, w0, h0, bop, depth); break;
          case 2: compose2(pDst1, outrow1, pDst2, outrow2, pTemp, rowbytes+1, w0, h0, bop, depth); break;
          case 3: compose3(pDst1, outrow1, pDst2, outrow2, pTemp, rowbytes+1, w0, h0, bop, depth); break;
          case 4: compose4(pDst1, outrow1,                 pTemp, rowbytes+1, w0, h0, bop, depth); break;
          case 6: compose6(                pDst2, outrow2, pTemp, rowbytes+1, w0, h0, bop, depth); break;
        }

        if (cur_frame == frames)
          res = 0;

        break;
      }
      else
      {
        c = (unsigned char)(chunk>>24);
        if (notabc(c)) break;
        c = (unsigned char)((chunk>>16) & 0xFF);
        if (notabc(c)) break;
        c = (unsigned char)((chunk>>8) & 0xFF);
        if (notabc(c)) break;
        c = (unsigned char)(chunk & 0xFF);
        if (notabc(c)) break;

        fseek(f1, len, SEEK_CUR);
        if (read32(&crc, f1)) break;
      }
    }

    *pWidth   = w;
    *pHeight  = h;
    *pColType = coltype;
    *pFirst   = first_visible;
    *pFrames  = frames;
    *pLoops   = loops;
    *ppOut1   = pOut1;
    *ppOut2   = pOut2;
    *ppDelays = pDelays;

    free(pData);
    free(pTemp);

  } while (0);

  inflateEnd(&zstream);
  fclose(f1);
  return res;
}

void write_chunk(FILE * f, const char * name, unsigned char * data, unsigned int length)
{
  unsigned int crc = crc32(0, Z_NULL, 0);
  unsigned int len = swap32(length);

  fwrite(&len, 1, 4, f);
  fwrite(name, 1, 4, f);
  crc = crc32(crc, (const Bytef *)name, 4);

  if (memcmp(name, "fdAT", 4) == 0)
  {
    unsigned int seq = swap32(next_seq_num++);
    fwrite(&seq, 1, 4, f);
    crc = crc32(crc, (const Bytef *)(&seq), 4);
    length -= 4;
  }

  if (data != NULL && length > 0)
  {
    fwrite(data, 1, length, f);
    crc = crc32(crc, data, length);
  }

  crc = swap32(crc);
  fwrite(&crc, 1, 4, f);
}

void write_IDATs(FILE * f, int frame, unsigned char * data, unsigned int length, unsigned int idat_size)
{
  unsigned int z_cmf = data[0];
  if ((z_cmf & 0x0f) == 8 && (z_cmf & 0xf0) <= 0x70)
  {
    if (length >= 2)
    {
      unsigned int z_cinfo = z_cmf >> 4;
      unsigned int half_z_window_size = 1 << (z_cinfo + 7);
      while (idat_size <= half_z_window_size && half_z_window_size >= 256)
      {
        z_cinfo--;
        half_z_window_size >>= 1;
      }
      z_cmf = (z_cmf & 0x0f) | (z_cinfo << 4);
      if (data[0] != (unsigned char)z_cmf)
      {
        data[0] = (unsigned char)z_cmf;
        data[1] &= 0xe0;
        data[1] += (unsigned char)(0x1f - ((z_cmf << 8) + data[1]) % 0x1f);
      }
    }
  }

  while (length > 0)
  {
    unsigned int ds = length;
    if (ds > PNG_ZBUF_SIZE)
      ds = PNG_ZBUF_SIZE;

    if (frame == 0)
      write_chunk(f, "IDAT", data, ds);
    else
      write_chunk(f, "fdAT", data, ds+4);

    data += ds;
    length -= ds;
  }
}

int get_rect(unsigned int w, unsigned int h, unsigned char *pimg1, unsigned char *pimg2, unsigned char *ptemp, unsigned int *px, unsigned int *py, unsigned int *pw, unsigned int *ph, unsigned int bpp, unsigned int has_tcolor, unsigned int tcolor)
{
  unsigned int   i, j;
  unsigned int   x_min = w-1;
  unsigned int   y_min = h-1;
  unsigned int   x_max = 0;
  unsigned int   y_max = 0;
  int   diffnum = 0;
  int   over_is_possible = 1;

  if (!has_tcolor)
    over_is_possible = 0;

  if (bpp == 1)
  {
    unsigned char *pa = pimg1;
    unsigned char *pb = pimg2;
    unsigned char *pc = ptemp;

    for (j=0; j<h; j++)
    for (i=0; i<w; i++)
    {
      unsigned char c = *pb++;
      if (*pa++ != c)
      {
        diffnum++;
        if ((has_tcolor) && (c == tcolor)) over_is_possible = 0;
        if (i<x_min) x_min = i;
        if (i>x_max) x_max = i;
        if (j<y_min) y_min = j;
        if (j>y_max) y_max = j;
      }
      else
        c = tcolor;

      *pc++ = c;
    }
  }
  else
  if (bpp == 2)
  {
    unsigned short *pa = (unsigned short *)pimg1;
    unsigned short *pb = (unsigned short *)pimg2;
    unsigned short *pc = (unsigned short *)ptemp;

    for (j=0; j<h; j++)
    for (i=0; i<w; i++)
    {
      unsigned int c1 = *pa++;
      unsigned int c2 = *pb++;
      if ((c1 != c2) && ((c1>>8) || (c2>>8)))
      {
        diffnum++;
        if ((c2 >> 8) != 0xFF) over_is_possible = 0;
        if (i<x_min) x_min = i;
        if (i>x_max) x_max = i;
        if (j<y_min) y_min = j;
        if (j>y_max) y_max = j;
      }
      else
        c2 = 0;

      *pc++ = c2;
    }
  }
  else
  if (bpp == 3)
  {
    unsigned char *pa = pimg1;
    unsigned char *pb = pimg2;
    unsigned char *pc = ptemp;

    for (j=0; j<h; j++)
    for (i=0; i<w; i++)
    {
      unsigned int c1 = (((pa[2]<<8)+pa[1])<<8)+pa[0];
      unsigned int c2 = (((pb[2]<<8)+pb[1])<<8)+pb[0];
      if (c1 != c2)
      {
        diffnum++;
        if ((has_tcolor) && (c2 == tcolor)) over_is_possible = 0;
        if (i<x_min) x_min = i;
        if (i>x_max) x_max = i;
        if (j<y_min) y_min = j;
        if (j>y_max) y_max = j;
      }
      else
        c2 = tcolor;

      memcpy(pc, &c2, 3);
      pa += 3;
      pb += 3;
      pc += 3;
    }
  }
  else
  if (bpp == 4)
  {
    unsigned int *pa = (unsigned int *)pimg1;
    unsigned int *pb = (unsigned int *)pimg2;
    unsigned int *pc = (unsigned int *)ptemp;

    for (j=0; j<h; j++)
    for (i=0; i<w; i++)
    {
      unsigned int c1 = *pa++;
      unsigned int c2 = *pb++;
      if ((c1 != c2) && ((c1>>24) || (c2>>24)))
      {
        diffnum++;
        if ((c2 >> 24) != 0xFF) over_is_possible = 0;
        if (i<x_min) x_min = i;
        if (i>x_max) x_max = i;
        if (j<y_min) y_min = j;
        if (j>y_max) y_max = j;
      }
      else
        c2 = 0;

      *pc++ = c2;
    }
  }

  if (diffnum == 0)
  {
    *px = *py = 0;
    *pw = *ph = 1; 
  }
  else
  {
    // webpmux can't handle unaligned frame positions.
    // We align them on a 2 pixel boundry here.
    // Related bug: https://code.google.com/p/webp/issues/detail?id=207
    int x_alignment_offset = x_min % 2 ? -1 : 0;
    int y_alignment_offset = y_min % 2 ? -1 : 0;
    
    x_min += x_alignment_offset;
    x_max += abs(x_alignment_offset);
    
    y_min += y_alignment_offset;
    y_max += abs(y_alignment_offset);
    
    *px = x_min;
    *py = y_min;
    *pw = x_max-x_min+1;
    *ph = y_max-y_min+1;
  }

  return over_is_possible;
}

void deflate_rect(unsigned char *pdata, int x, int y, int w, int h, int bpp, int stride, int zbuf_size, int n)
{
  int i, j, v;
  int a, b, c, pa, pb, pc, p;
  int rowbytes = w * bpp;
  unsigned char * prev = NULL;
  unsigned char * row  = pdata + y*stride + x*bpp;
  unsigned char * out;

  op[n*2].valid = 1;
  op[n*2].zstream.next_out = op[n*2].zbuf;
  op[n*2].zstream.avail_out = zbuf_size;

  op[n*2+1].valid = 1;
  op[n*2+1].zstream.next_out = op[n*2+1].zbuf;
  op[n*2+1].zstream.avail_out = zbuf_size;

  for (j=0; j<h; j++)
  {
    unsigned int    sum = 0;
    unsigned char * best_row = row_buf;
    unsigned int    mins = ((unsigned int)(-1)) >> 1;

    out = row_buf+1;
    for (i=0; i<rowbytes; i++)
    {
      v = out[i] = row[i];
      sum += (v < 128) ? v : 256 - v;
    }
    mins = sum;

    sum = 0;
    out = sub_row+1;
    for (i=0; i<bpp; i++)
    {
      v = out[i] = row[i];
      sum += (v < 128) ? v : 256 - v;
    }
    for (i=bpp; i<rowbytes; i++)
    {
      v = out[i] = row[i] - row[i-bpp];
      sum += (v < 128) ? v : 256 - v;
      if (sum > mins) break;
    }
    if (sum < mins)
    {
      mins = sum;
      best_row = sub_row;
    }

    if (prev)
    {
      sum = 0;
      out = up_row+1;
      for (i=0; i<rowbytes; i++)
      {
        v = out[i] = row[i] - prev[i];
        sum += (v < 128) ? v : 256 - v;
        if (sum > mins) break;
      }
      if (sum < mins)
      {
        mins = sum;
        best_row = up_row;
      }

      sum = 0;
      out = avg_row+1;
      for (i=0; i<bpp; i++)
      {
        v = out[i] = row[i] - prev[i]/2;
        sum += (v < 128) ? v : 256 - v;
      }
      for (i=bpp; i<rowbytes; i++)
      {
        v = out[i] = row[i] - (prev[i] + row[i-bpp])/2;
        sum += (v < 128) ? v : 256 - v;
        if (sum > mins) break;
      }
      if (sum < mins)
      { 
        mins = sum;
        best_row = avg_row;
      }

      sum = 0;
      out = paeth_row+1;
      for (i=0; i<bpp; i++)
      {
        v = out[i] = row[i] - prev[i];
        sum += (v < 128) ? v : 256 - v;
      }
      for (i=bpp; i<rowbytes; i++)
      {
        a = row[i-bpp];
        b = prev[i];
        c = prev[i-bpp];
        p = b - c;
        pc = a - c;
        pa = abs(p);
        pb = abs(pc);
        pc = abs(p + pc);
        p = (pa <= pb && pa <=pc) ? a : (pb <= pc) ? b : c;
        v = out[i] = row[i] - p;
        sum += (v < 128) ? v : 256 - v;
        if (sum > mins) break;
      }
      if (sum < mins)
      {
        best_row = paeth_row;
      }
    }

    op[n*2].zstream.next_in = row_buf;
    op[n*2].zstream.avail_in = rowbytes + 1;
    deflate(&op[n*2].zstream, Z_NO_FLUSH);

    op[n*2+1].zstream.next_in = best_row;
    op[n*2+1].zstream.avail_in = rowbytes + 1;
    deflate(&op[n*2+1].zstream, Z_NO_FLUSH);

    prev = row;
    row += stride;
  }

  deflate(&op[n*2].zstream, Z_FINISH);
  deflate(&op[n*2+1].zstream, Z_FINISH);

  op[n*2].x = op[n*2+1].x = x;
  op[n*2].y = op[n*2+1].y = y;
  op[n*2].w = op[n*2+1].w = w;
  op[n*2].h = op[n*2+1].h = h;
}

void SaveAPNG(char * szOut, unsigned char * pdata, unsigned short * delays, unsigned int w, unsigned int h, unsigned int first, unsigned int frames, unsigned int loops, unsigned char coltype)
{
  unsigned int     i, j, k, c;
  unsigned int     bpp, rowbytes, imagesize, imgstride;
  unsigned int     idat_size, zbuf_size, zsize;
  unsigned int     x0, y0, w0, h0, x1, y1, w1, h1, try_over;
  unsigned int     has_tcolor, tcolor;
  unsigned char    dop, bop, r, g, b;
  unsigned char    bits[256];
  unsigned char  * bigcube;
  unsigned char  * zbuf;
  unsigned char  * tmpframe;
  unsigned char  * prev_frame;
  unsigned char  * cur_frame;
  unsigned char  * next_frame;
  unsigned char  * sp;
  unsigned char  * dp;
  FILE           * f;

  bigcube = (unsigned char *)malloc(256*256*32);
  if (!bigcube)
  {
    printf( "Error: not enough memory\n" );
    return;
  }

  memset(bigcube, 0, 256*256*32);

  for (i=0; i<256; i++)
    bits[i] = ((i>>7)&1) + ((i>>6)&1) + ((i>>5)&1) + ((i>>4)&1) + ((i>>3)&1) + ((i>>2)&1) + ((i>>1)&1) + (i&1);
  
  bpp = 1;
  if (coltype == 2)
    bpp = 3;
  else
  if (coltype == 4)
    bpp = 2;
  else
  if (coltype == 6)
    bpp = 4;

  rowbytes  = w * bpp;
  imagesize = rowbytes * h;
  idat_size = (rowbytes + 1) * h;
  zbuf_size = idat_size + ((idat_size + 7) >> 3) + ((idat_size + 63) >> 6) + 11;
  imgstride = imagesize;

  /* Optimizations - start */
  has_tcolor = 0;
  tcolor = 0;

  if (coltype == 6)
  {
    int transparency = 0;
    int partial_transparency = 0;

    has_tcolor = 1;
    for (i=first; i<=frames; i++)
    {
      sp = pdata + imgstride*i;
      for (j=0; j<w*h; j++, sp+=4)
      {
        if (sp[3] != 255)
        {
          transparency = 1;
          if (sp[3] == 0)
            sp[0] = sp[1] = sp[2] = 0;
          else
            partial_transparency = 1;
        }
      }
    }

    if (!keep_coltype)
    {
      if (transparency == 0)
      {
        trnssize = 0;
        has_tcolor = 0;
        coltype = 2;
        bpp = 3;
        rowbytes  = w * bpp;
        imagesize = rowbytes * h;
        for (i=first; i<=frames; i++)
        {
          sp = dp = pdata + imgstride*i;
          for (j=0; j<w*h; j++)
          {
            *dp++ = *sp++;
            *dp++ = *sp++;
            *dp++ = *sp++;
            sp++;
          }
        }
      }
      else
      if (partial_transparency == 0)
      {
        int colors = 0;
        for (i=first; i<=frames; i++)
        {
          sp = pdata + imgstride*i;
          for (j=0; j<w*h; j++)
          {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            c = (b<<16) + (g<<8) + r;
            if (*sp++ != 0)
              bigcube[c>>3] |= (1<<(r & 7));
          }
        }
        sp = bigcube;
        for (i=0; i<256*256*32; i++)
          colors += bits[*sp++];

        if (colors + has_tcolor <= 256)
        {
          pal[0][0] = pal[0][1] = pal[0][2] = trns[0] = 0;
          palsize = trnssize = 1;
          for (i=0; i<256*256*32; i++)
          if ((c = bigcube[i]) != 0)
          {
            for (k=0; k<8; k++, c>>=1)
            if (c&1)
            {
              pal[palsize][2] = i>>13;
              pal[palsize][1] = (i>>5)&255;
              pal[palsize][0] = ((i&31)<<3) + k;
              palsize++;
              trns[trnssize++] = 255;
            }
          }
          coltype = 3;
          bpp = 1;
          rowbytes  = w;
          imagesize = rowbytes * h;
          for (i=first; i<=frames; i++)
          {
            sp = dp = pdata + imgstride*i;
            for (j=0; j<w*h; j++)
            {
              r = *sp++;
              g = *sp++;
              b = *sp++;
              if (*sp++ != 0)
              {
                for (c=1; c<palsize; c++)
                  if (r==pal[c][0] && g==pal[c][1] && b==pal[c][2])
                    break;
                *dp++ = c;
              }
              else
                *dp++ = 0;
            }
          }
        }
      }
    }
  }

  if (coltype == 2)
  {
    int colors = 0;
    if (trnssize)
    {
      has_tcolor = 1;
      tcolor = (((trns[5]<<8)+trns[3])<<8)+trns[1];
    }

    for (i=first; i<=frames; i++)
    {
      sp = pdata + imgstride*i;
      for (j=0; j<w*h; j++)
      {
        r = *sp++;
        g = *sp++;
        b = *sp++;
        c = (b<<16) + (g<<8) + r;
        if (has_tcolor == 0 || c != tcolor)
          bigcube[c>>3] |= (1<<(r & 7));
      }
    }

    sp = bigcube;
    for (i=0; i<256*256*32; i++)
    {
      c = *sp++;
      if (has_tcolor == 0 && c == 0)
      {
        has_tcolor = 1;
        tcolor = i<<3;
        trnssize = 6;
        trns[0] = 0; trns[1] = (i&31)<<3;
        trns[2] = 0; trns[3] = (i>>5)&255;
        trns[4] = 0; trns[5] = i>>13;
      } 
      colors += bits[c];
    }

    if (!keep_coltype)
    {
      if (colors + has_tcolor <= 256)
      {
        palsize = trnssize = 0;
        if (has_tcolor == 1)
        {
          pal[0][0] = tcolor&255;
          pal[0][1] = (tcolor>>8)&255;
          pal[0][2] = (tcolor>>16)&255;
          trns[0] = 0;
          palsize = trnssize = 1;
        }
        for (i=0; i<256*256*32; i++)
        if ((c = bigcube[i]) != 0)
        {
          for (k=0; k<8; k++, c>>=1)
          if (c&1)
          {
            pal[palsize][2] = i>>13;
            pal[palsize][1] = (i>>5)&255;
            pal[palsize][0] = ((i&31)<<3) + k;
            palsize++;
            trns[trnssize++] = 255;
          }
        }
        coltype = 3;
        bpp = 1;
        rowbytes  = w;
        imagesize = rowbytes * h;
        for (i=first; i<=frames; i++)
        {
          sp = dp = pdata + imgstride*i;
          for (j=0; j<w*h; j++)
          {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            for (c=0; c<palsize; c++)
              if (r==pal[c][0] && g==pal[c][1] && b==pal[c][2])
                break;
            *dp++ = c;
          }
        }
      }
    }
  }

  if (coltype == 3)
  {
    for (i=0; i<256; i++)
    {
      stats[i].i = i;
      stats[i].num = 0;
    }

    for (i=0; i<trnssize; i++)
    if (trns[i] == 0)
    {
      stats[i].trans = 1;
      has_tcolor = 1;
      tcolor = i;
      break;
    }

    for (i=first; i<=frames; i++)
    {
      sp = pdata + imgstride*i;
      for (j=0; j<imagesize; j++)
        stats[*sp++].num++;
    }

    if (!has_tcolor)
    for (i=0; i<256; i++)
    if (stats[i].num == 0)
    {
      trns[i] = 0;
      stats[i].trans = 1;
      has_tcolor = 1;
      tcolor = i;
      break;
    }

    if (!keep_palette)
    {
      unsigned char pl[256][3];
      unsigned char tr[256];
      unsigned int  sh[256];

      /* sort the palette, by color usage */
      qsort(&stats[0], 256, sizeof(STATS), cmp_stats);
      tcolor = 0;

      palsize = trnssize = 0;
      for (i=0; i<256; i++)
      {
        pl[i][0] = pal[stats[i].i][0];
        pl[i][1] = pal[stats[i].i][1];
        pl[i][2] = pal[stats[i].i][2];
        tr[i]    = trns[stats[i].i];

        if (stats[i].num != 0)
          palsize = i+1;

        if ((tr[i] != 255) && (stats[i].num != 0 || i==0))
          trnssize = i+1;

        sh[stats[i].i] = i;
      }

      if (palsize) memcpy(pal, pl, palsize*3);
      if (trnssize) memcpy(trns, tr, trnssize);

      for (i=first; i<=frames; i++)
      {
        sp = pdata + imgstride*i;
        for (j=0; j<imagesize; j++)
          sp[j] = sh[sp[j]];
      }
    }
  }

  if (coltype == 4)
  {
    int transparency = 0;

    has_tcolor = 1;
    for (i=first; i<=frames; i++)
    {
      sp = pdata + imgstride*i;
      for (j=0; j<w*h; j++, sp+=2)
      {
        if (sp[1] != 255)
        {
          transparency = 1;
          if (sp[1] == 0)
            sp[0] = 0;
        }
      }
    }

    if (!keep_coltype)
    {
      if (transparency == 0)
      {
        trnssize = 0;
        has_tcolor = 0;
        coltype = 0;
        bpp = 1;
        rowbytes  = w;
        imagesize = rowbytes * h;
        for (i=first; i<=frames; i++)
        {
          sp = dp = pdata + imgstride*i;
          for (j=0; j<w*h; j++)
          {
            *dp++ = *sp++;
            sp++;
          }
        }
      }
    }
  }

  if (coltype == 0)
  {
    if (trnssize)
    {
      has_tcolor = 1;
      tcolor = trns[1];
    }
    else
    {
      for (i=0; i<256; i++)
        stats[i].num = 0;

      for (i=first; i<=frames; i++)
      {
        sp = pdata + imgstride*i;
        for (j=0; j<imagesize; j++)
          stats[*sp++].num++;
      }

      for (i=0; i<256; i++)
      if (stats[i].num == 0)
      {
        has_tcolor = 1;
        tcolor = i;
        trns[0] = 0;
        trns[1] = tcolor;
        trnssize = 2;
        break;
      }
    }
  }
  /* Optimizations - end */

  for (i=0; i<12; i++)
  {
    op[i].zstream.data_type = Z_BINARY;
    op[i].zstream.zalloc = Z_NULL;
    op[i].zstream.zfree = Z_NULL;
    op[i].zstream.opaque = Z_NULL;

    if (i & 1)
      deflateInit2(&op[i].zstream, Z_BEST_COMPRESSION, 8, 15, 8, Z_FILTERED);
    else
      deflateInit2(&op[i].zstream, Z_BEST_COMPRESSION, 8, 15, 8, Z_DEFAULT_STRATEGY);

    op[i].zbuf = (unsigned char *)malloc(zbuf_size);
    if (op[i].zbuf == NULL)
    {
      printf( "Error: not enough memory\n" );
      return;
    }
  }

  tmpframe = (unsigned char *)malloc(imagesize);
  zbuf = (unsigned char *)malloc(zbuf_size);
  row_buf = (unsigned char *)malloc(rowbytes + 1);
  sub_row = (unsigned char *)malloc(rowbytes + 1);
  up_row = (unsigned char *)malloc(rowbytes + 1);
  avg_row = (unsigned char *)malloc(rowbytes + 1);
  paeth_row = (unsigned char *)malloc(rowbytes + 1);

  if (tmpframe && zbuf && row_buf && sub_row && up_row && avg_row && paeth_row)
  {
    row_buf[0] = 0;
    sub_row[0] = 1;
    up_row[0] = 2;
    avg_row[0] = 3;
    paeth_row[0] = 4;
  }
  else
  {
    printf( "Error: not enough memory\n" );
    return;
  }

  if ((f = fopen(szOut, "wb")) != 0)
  {
    struct IHDR 
    {
      unsigned int    mWidth;
      unsigned int    mHeight;
      unsigned char   mDepth;
      unsigned char   mColorType;
      unsigned char   mCompression;
      unsigned char   mFilterMethod;
      unsigned char   mInterlaceMethod;
    } ihdr = { swap32(w), swap32(h), 8, coltype, 0, 0, 0 };

    struct acTL 
    {
      unsigned int    mFrameCount;
      unsigned int    mLoopCount;
    } actl = { swap32(frames), swap32(loops) };

    struct fcTL 
    {
      unsigned int    mSeq;
      unsigned int    mWidth;
      unsigned int    mHeight;
      unsigned int    mXOffset;
      unsigned int    mYOffset;
      unsigned short  mDelayNum;
      unsigned short  mDelayDen;
      unsigned char   mDisposeOp;
      unsigned char   mBlendOp;
    } fctl;

    fwrite(png_sign, 1, 8, f);

    write_chunk(f, "IHDR", (unsigned char *)(&ihdr), 13);

    if (frames > first)
      write_chunk(f, "acTL", (unsigned char *)(&actl), 8);

    if (palsize > 0)
      write_chunk(f, "PLTE", (unsigned char *)(&pal), palsize*3);

    if (trnssize > 0)
      write_chunk(f, "tRNS", trns, trnssize);

    x0 = 0;
    y0 = 0;
    w0 = w;
    h0 = h;
    bop = 0;

    if (first == 0)
    {
      printf("saving frame %d of %d\n", 0, frames);
      deflate_rect(pdata, x0, y0, w0, h0, bpp, rowbytes, zbuf_size, 0);

      if (op[0].zstream.total_out <= op[1].zstream.total_out)
      {
        zsize = op[0].zstream.total_out;
        memcpy(zbuf, op[0].zbuf, zsize);
      }
      else
      {
        zsize = op[1].zstream.total_out;
        memcpy(zbuf, op[1].zbuf, zsize);
      }

      deflateReset(&op[0].zstream);
      op[0].zstream.data_type = Z_BINARY;
      deflateReset(&op[1].zstream);
      op[1].zstream.data_type = Z_BINARY;
      write_IDATs(f, 0, zbuf, zsize, idat_size);
    }

    cur_frame  = pdata;
    next_frame = pdata + imgstride;

    if (frames > 0)
    {
      printf("saving frame %d of %d\n", 1, frames);
      deflate_rect(next_frame, x0, y0, w0, h0, bpp, rowbytes, zbuf_size, 0);

      if (op[0].zstream.total_out <= op[1].zstream.total_out)
      {
        zsize = op[0].zstream.total_out;
        memcpy(zbuf, op[0].zbuf, zsize);
      }
      else
      {
        zsize = op[1].zstream.total_out;
        memcpy(zbuf, op[1].zbuf, zsize);
      }

      deflateReset(&op[0].zstream);
      op[0].zstream.data_type = Z_BINARY;
      deflateReset(&op[1].zstream);
      op[1].zstream.data_type = Z_BINARY;

      for (i=1; i<frames; i++)
      {
        unsigned int  op_min;
        int           op_best;

        prev_frame  = cur_frame;
        cur_frame   = next_frame;
        next_frame += imgstride;

        printf("saving frame %d of %d\n", i+1, frames);

        for (j=0; j<12; j++)
          op[j].valid = 0;

        /* dispose = none */
        try_over = get_rect(w, h, cur_frame, next_frame, tmpframe, &x1, &y1, &w1, &h1, bpp, has_tcolor, tcolor);
        deflate_rect(next_frame, x1, y1, w1, h1, bpp, rowbytes, zbuf_size, 0);
        if (try_over)
          deflate_rect(tmpframe, x1, y1, w1, h1, bpp, rowbytes, zbuf_size, 1);

        /* dispose = background */
        if (has_tcolor)
        {
          memcpy(tmpframe, cur_frame, imagesize);
          if (coltype == 2)
            for (j=0; j<h0; j++)
              for (k=0; k<w0; k++)
                memcpy(tmpframe + ((j+y0)*w + (k+x0))*3, &tcolor, 3);
          else
            for (j=0; j<h0; j++)
              memset(tmpframe + ((j+y0)*w + x0)*bpp, tcolor, w0*bpp);

          try_over = get_rect(w, h, tmpframe, next_frame, tmpframe, &x1, &y1, &w1, &h1, bpp, has_tcolor, tcolor);

          deflate_rect(next_frame, x1, y1, w1, h1, bpp, rowbytes, zbuf_size, 2);
          if (try_over)
            deflate_rect(tmpframe, x1, y1, w1, h1, bpp, rowbytes, zbuf_size, 3);
        }

/*        if (i > 1)*/
/*        {*/
/*          // dispose = previous*/
/*          try_over = get_rect(w, h, prev_frame, next_frame, tmpframe, &x1, &y1, &w1, &h1, bpp, has_tcolor, tcolor);*/
/*          deflate_rect(next_frame, x1, y1, w1, h1, bpp, rowbytes, zbuf_size, 4);*/
/*          if (try_over)*/
/*            deflate_rect(tmpframe, x1, y1, w1, h1, bpp, rowbytes, zbuf_size, 5);*/
/*        }*/

        op_min = op[0].zstream.total_out;
        op_best = 0;
        for (j=1; j<12; j++)
        {
          if (op[j].valid)
          {
            if (op[j].zstream.total_out < op_min)
            {
              op_min = op[j].zstream.total_out;
              op_best = j;
            }
          }
        }

        dop = op_best >> 2;

        fctl.mSeq       = swap32(next_seq_num++);
        fctl.mWidth     = swap32(w0);
        fctl.mHeight    = swap32(h0);
        fctl.mXOffset   = swap32(x0);
        fctl.mYOffset   = swap32(y0);
        fctl.mDelayNum  = swap16(delays[i*2]);
        fctl.mDelayDen  = swap16(delays[i*2+1]);
        fctl.mDisposeOp = dop;
        fctl.mBlendOp   = bop;
        write_chunk(f, "fcTL", (unsigned char *)(&fctl), 26);

        write_IDATs(f, i-first, zbuf, zsize, idat_size);

        /* process apng dispose - begin */
        if (dop == 1)
        {
          if (coltype == 2)
            for (j=0; j<h0; j++)
              for (k=0; k<w0; k++)
                memcpy(cur_frame + ((j+y0)*w + (k+x0))*3, &tcolor, 3);
          else
            for (j=0; j<h0; j++)
              memset(cur_frame + ((j+y0)*w + x0)*bpp, tcolor, w0*bpp);
        }
        else
        if (dop == 2)
        {
          for (j=0; j<h0; j++)
            memcpy(cur_frame + ((j+y0)*w + x0)*bpp, prev_frame + ((j+y0)*w + x0)*bpp, w0*bpp);
        }
        /* process apng dispose - end */

        x0 = op[op_best].x;
        y0 = op[op_best].y;
        w0 = op[op_best].w;
        h0 = op[op_best].h;
        bop = (op_best >> 1) & 1;

        zsize = op[op_best].zstream.total_out;
        memcpy(zbuf, op[op_best].zbuf, zsize);

        for (j=0; j<12; j++)
        {
          deflateReset(&op[j].zstream);
          op[j].zstream.data_type = Z_BINARY;
        }
      }

      if (frames > first)
      {
        fctl.mSeq       = swap32(next_seq_num++);
        fctl.mWidth     = swap32(w0);
        fctl.mHeight    = swap32(h0);
        fctl.mXOffset   = swap32(x0);
        fctl.mYOffset   = swap32(y0);
        fctl.mDelayNum  = swap16(delays[frames*2]);
        fctl.mDelayDen  = swap16(delays[frames*2+1]);
        fctl.mDisposeOp = 0;
        fctl.mBlendOp   = bop;
        write_chunk(f, "fcTL", (unsigned char *)(&fctl), 26);
      }

      write_IDATs(f, frames-first, zbuf, zsize, idat_size);
    }
    write_chunk(f, "IEND", 0, 0);
    fclose(f);
  }
  else
  {
    printf( "Error: couldn't open file for writing\n" );
    return;
  }

  for (i=0; i<12; i++)
  {
    deflateEnd(&op[i].zstream);
    if (op[i].zbuf != NULL)
      free(op[i].zbuf);
  }

  free(tmpframe);
  free(zbuf);
  free(row_buf);
  free(sub_row);
  free(up_row);
  free(avg_row);
  free(paeth_row);
  free(bigcube);
}

int main(int argc, char** argv)
{
  char           * szIn;
  char             szOut[256];
  char           * szExt;
  unsigned int     w, h, first, frames, loops;
  unsigned char    coltype;
  unsigned char  * pOut1 = NULL;
  unsigned char  * pOut2 = NULL;
  unsigned short * pDelays = NULL;

  printf("\nAPNG Optimizer 1.0\n\n");

  if (argc <= 1)
  {
    printf("Usage: apngopt anim.png [anim.opt.png]\n");
    return 1;
  }

  szIn = argv[1];

  if (argc > 2)
  {
    strncpy(szOut, argv[2], 255);
    szOut[255] = '\0';
  }
  else
  {
    strcpy(szOut, szIn);
    if ((szExt = strrchr(szOut, '.')) != NULL) *szExt = 0;
    strcat(szOut, ".opt.png");
  }

  if (LoadAPNG(szIn, &w, &h, &coltype, &first, &frames, &loops, &pOut1, &pOut2, &pDelays) != 0)
  {
    printf("Error: can't load '%s'\n", szIn);
    return 1;
  }

  if (coltype == 6)
    SaveAPNG(szOut, pOut2, pDelays, w, h, first, frames, loops, 6);
  else
  if (coltype == 4)
    SaveAPNG(szOut, pOut1, pDelays, w, h, first, frames, loops, coltype);
  else
  if (keep_original)
    SaveAPNG(szOut, pOut1, pDelays, w, h, first, frames, loops, coltype);
  else
    SaveAPNG(szOut, pOut2, pDelays, w, h, first, frames, loops, 6);

  free(pOut1);
  free(pOut2);
  free(pDelays);

  printf("all done\n");
    
  return 0;
}
