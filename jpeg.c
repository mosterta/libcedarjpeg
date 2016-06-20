/*
 * Simple JPEG parser (only basic functions)
 *
 * Copyright (c) 2013 Jens Kuske <jenskuske@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include "jpeg.h"

#define M_SOF0  0xc0
#define M_SOF1  0xc1
#define M_SOF2  0xc2
#define M_SOF3  0xc3
#define M_SOF5  0xc5
#define M_SOF6  0xc6
#define M_SOF7  0xc7
#define M_SOF9  0xc9
#define M_SOF10 0xca
#define M_SOF11 0xcb
#define M_SOF13 0xcd
#define M_SOF14 0xce
#define M_SOF15 0xcf
#define M_SOI   0xd8
#define M_EOI   0xd9
#define M_SOS   0xda
#define M_DQT   0xdb
#define M_DRI   0xdd
#define M_DHT   0xc4
#define M_DAC   0xcc
#define M_APP1	0xe1

#define EXIF_TAG_ORIENTATION    0x0112

const char comp_types[5][3] = { "Y", "Cb", "Cr" };

inline static uint8_t READ8(const uint8_t * p, uint16_t *pos)
{
  uint8_t r = p[*pos];
  *pos += 1;
  return r;
}

inline static uint16_t READ16(const uint8_t * p, uint16_t *pos)
{
  uint16_t r = (p[*pos] << 8) | p[*pos+1];
  *pos += 2;
  return r;
}

inline static uint32_t READ32(const uint8_t * p, uint16_t *pos)
{
  uint32_t r = (p[*pos] << 24) | (p[*pos+1] << 16) | (p[*pos+2] << 8) | p[*pos+3];
  *pos += 4;
  return r;
}

static int process_dqt(struct jpeg_t *jpeg, const uint8_t *data, const int len)
{
	int pos;

	for (pos = 0; pos < len; pos += 65)
	{
		if ((data[pos] >> 4) != 0)
		{
			fprintf(stderr, "Only 8bit Quantization Tables supported\n");
			return 0;
		}
		jpeg->quant[data[pos] & 0x03] = (struct quant_t *)&(data[pos + 1]);
	}

	return 1;
}

static int process_dht(struct jpeg_t *jpeg, const uint8_t *data, const int len)
{
	int pos = 0;

	while (pos < len)
	{
		uint8_t id = ((data[pos] & 0x03) << 1) | ((data[pos] & 0x10) >> 4);

		jpeg->huffman[id] = (struct huffman_t *)&(data[pos + 1]);

		int i;
		pos += 17;
		for (i = 0; i < 16; i++)
			pos += jpeg->huffman[id]->num[i];
	}
	return 1;
}
static int process_sof0(struct jpeg_t *jpeg, const uint8_t *data, const int len)
{
  int i;
  int pos = 0;
  
  jpeg->bits = data[pos];
  jpeg->width = ((uint16_t)data[pos + 3]) << 8 | (uint16_t)data[pos + 4];
  jpeg->height = ((uint16_t)data[pos + 1]) << 8 | (uint16_t)data[pos + 2];
  for (i = 0; i < data[pos + 5]; i++)
  {
    uint8_t id = data[pos + 6 + 3 * i] - 1;
    if (id > 2)
    {
      fprintf(stderr, "only YCbCr supported\n");
      return 0;
    }
    jpeg->comp[id].samp_h = data[pos + 7 + 3 * i] >> 4;
    jpeg->comp[id].samp_v = data[pos + 7 + 3 * i] & 0x0f;
    jpeg->comp[id].qt = data[pos + 8 + 3 * i];
    if( id == 0 )
    {
      jpeg->color_format = (jpeg->comp[0].samp_h << 4) | jpeg->comp[0].samp_v;

    }
  }
  return 1;
}

static int process_exif(struct jpeg_t *jpeg, const uint8_t *data, const int len)
{
	uint16_t pos = 0;

        // Exif header
        if(READ32(data, &pos) == 0x45786966)
        {
          int bMotorolla = 0;
          int bError = 0;
          pos += 2;
        
          char o1 = READ8(data, &pos);
          char o2 = READ8(data, &pos);

          /* Discover byte order */
          if(o1 == 'M' && o2 == 'M')
            bMotorolla = 1;
          else if(o1 == 'I' && o2 == 'I')
            bMotorolla = 0;
          else
            bError = 1;
        
          pos += 2;

          if(!bError)
          {
            unsigned int offset, a, b, numberOfTags, tagNumber;
  
            // Get first IFD offset (offset to IFD0)
            if(bMotorolla)
            {
              pos += 2;

              a = READ8(data, &pos);
              b = READ8(data, &pos);
              offset = (a << 8) + b;
            }
            else
            {
              a = READ8(data, &pos);
              b = READ8(data, &pos);
              offset = (b << 8) + a;

              pos += 2;
            }

            offset -= 8;
            if(offset > 0)
            {
              pos += offset;
            } 

            // Get the number of directory entries contained in this IFD
            if(bMotorolla)
            {
              a = READ8(data, &pos);
              b = READ8(data, &pos);
              numberOfTags = (a << 8) + b;
            }
            else
            {
              a = READ8(data, &pos);
              b = READ8(data, &pos);
              numberOfTags = (b << 8) + a;
            }

            while(numberOfTags && pos < len)
            {
              // Get Tag number
              if(bMotorolla)
              {
                a = READ8(data, &pos);
                b = READ8(data, &pos);
                tagNumber = (a << 8) + b;
              }
              else
              {
                a = READ8(data, &pos);
                b = READ8(data, &pos);
                tagNumber = (b << 8) + a;
              }

              //found orientation tag
              if(tagNumber == EXIF_TAG_ORIENTATION)
              {
                if(bMotorolla)
                {
                  pos += 7;
                  jpeg->orientation = READ8(data, &pos)-1;
                  pos += 2;
                }
                else
                {
                  pos += 6;
                  jpeg->orientation = READ8(data, &pos)-1;
                  pos += 3;
                }
                break;
              }
              else
              {
                pos += 10;
              }
              numberOfTags--;
            }
          }
	  else 
	    return 0;
        }
	else
	  return 0;
	return 1;	
}

int parse_jpeg(struct cedarJpeg_handle *jpeg, const uint8_t *data, const int len)
{
	if (len < 2 || data[0] != 0xff || data[1] != M_SOI)
		return -1;

	int pos = 2;
	int sos = 0;
	while (!sos)
	{
		int i;

		while (pos < len && data[pos] == 0xff)
			pos++;

		if (!(pos + 2 < len))
			return -2;

		uint8_t marker = data[pos++];
		uint16_t seg_len = ((uint16_t)data[pos]) << 8 | (uint16_t)data[pos + 1];

		if (!(pos + seg_len < len))
			return -3;

		switch (marker)
		{
		case M_DQT:
			if (!process_dqt(&jpeg->jpeg, &data[pos + 2], seg_len - 2))
				return -4;

			break;

		case M_DHT:
			if (!process_dht(&jpeg->jpeg, &data[pos + 2], seg_len - 2))
				return -5;

			break;

		case M_SOF0:
            if (!process_sof0(&jpeg->jpeg, &data[pos +2], seg_len -2))
                return -6;
            break;
			break;

		case M_DRI:
			jpeg->jpeg.restart_interval = ((uint16_t)data[pos + 2]) << 8 | (uint16_t)data[pos + 3];
			break;

		case M_SOS:
			for (i = 0; i < data[pos + 2]; i++)
			{
				uint8_t id = data[pos + 3 + 2 * i] - 1;
				if (id > 2)
				{
					fprintf(stderr, "only YCbCr supported\n");
					return -7;
				}
				jpeg->jpeg.comp[id].ht_dc = data[pos + 4 + 2 * i] >> 4;
				jpeg->jpeg.comp[id].ht_ac = data[pos + 4 + 2 * i] & 0x0f;
			}
			sos = 1;
			break;

		case M_DAC:
			fprintf(stderr, "Arithmetic Coding unsupported\n");
			return -8;

		case M_SOF2:
          fprintf(stderr, "only Baseline DCT supported (yet?), marker %d not supported\n", marker);
          //if (!process_sof0(&jpeg->jpeg, &data[pos +2], seg_len -2))
            return -6;
          break;
          
        case M_SOF1:
        case M_SOF3:
		case M_SOF5:
		case M_SOF6:
		case M_SOF7:
		case M_SOF9:
		case M_SOF10:
		case M_SOF11:
		case M_SOF13:
		case M_SOF14:
		case M_SOF15:
			fprintf(stderr, "only Baseline DCT supported (yet?), marker %d not supported\n", marker);
			return -9;

		case M_SOI:
		case M_EOI:
			fprintf(stderr, "corrupted file\n");
			return -10;

		case M_APP1:
			if(!process_exif(&jpeg->jpeg, &data[pos + 2], seg_len - 2))
				; //return -11;
			break;

		default:
			//fprintf(stderr, "unknown marker: 0x%02x len: %u\n", marker, seg_len);
			break;
		}
		pos += seg_len;
	}

	jpeg->data = (uint8_t *)&(data[pos]);
	jpeg->data_len = len - pos;

	return 1;
}
#if 0
void dump_jpeg(const struct jpeg_t *jpeg)
{
	int i, j, k;
	printf("Width: %u  Height: %u  Bits: %u\nRestart interval: %u\nComponents:\n", jpeg->width, jpeg->height, jpeg->bits, jpeg->restart_interval);
	for (i = 0; i < 3; i++)
	{
		if (jpeg->comp[i].samp_h && jpeg->comp[i].samp_v)
			printf("\tType: %s  Sampling: %u:%u  QT: %u  HT: %u/%u\n", comp_types[i], jpeg->comp[i].samp_h, jpeg->comp[i].samp_v, jpeg->comp[i].qt, jpeg->comp[i].ht_dc, jpeg->comp[i].ht_dc);
	}
	printf("Quantization Tables:\n");
	for (i = 0; i < 4; i++)
	{
		if (jpeg->quant[i])
		{
			printf("\tID: %u\n", i);
			for (j = 0; j < 64 / 8; j++)
			{
				printf("\t\t");
				for (k = 0; k < 8; k++)
				{
					printf("0x%02x ", jpeg->quant[i]->coeff[j * 8 + k]);
				}
				printf("\n");
			}
		}
	}
	printf("Huffman Tables:\n");
	for (i = 0; i < 8; i++)
	{
		if (jpeg->huffman[i])
		{
			printf("\tID: %u (%cC)\n", (i & 0x06) >> 1, i & 0x01 ? 'A' : 'D');
			int sum = 0;
			for (j = 0; j < 16; j++)
			{
				if (jpeg->huffman[i]->num[j])
				{
					printf("\t\t%u bits:", j + 1);
					for (k = 0; k < jpeg->huffman[i]->num[j]; k++)
					{
						printf(" 0x%02x", jpeg->huffman[i]->codes[sum + k]);
					}
					printf("\n");
					sum += jpeg->huffman[i]->num[j];
				}
			}
		}
	}
	printf("Data length: %u\n", jpeg->data_len);
}
#endif
