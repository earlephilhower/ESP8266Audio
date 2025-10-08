/*
    libmad - MPEG audio decoder library
    Copyright (C) 2000-2004 Underbit Technologies, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    $Id: stream.c,v 1.12 2004/02/05 09:02:39 rob Exp $
*/

#pragma GCC optimize ("O3")

#include <pgmspace.h>
#  include "config.h"

# include "global.h"

# include <stdlib.h>

# include "bit.h"
# include "stream.h"

/*
    NAME:	stream->init()
    DESCRIPTION:	initialize stream struct
*/
void mad_stream_init(struct mad_stream *stream) {
    stack(__FUNCTION__, __FILE__, __LINE__);
    stream->buffer     = 0;
    stream->bufend     = 0;
    stream->skiplen    = 0;

    stream->sync       = 0;
    stream->freerate   = 0;

    stream->this_frame = 0;
    stream->next_frame = 0;
    mad_bit_init(&stream->ptr, 0);

    mad_bit_init(&stream->anc_ptr, 0);
    stream->anc_bitlen = 0;

    stream->md_len     = 0;

    stream->options    = 0;
    stream->error      = MAD_ERROR_NONE;
}

/*
    NAME:	stream->finish()
    DESCRIPTION:	deallocate any dynamic memory associated with stream
*/
void mad_stream_finish(struct mad_stream *stream) {
    stack(__FUNCTION__, __FILE__, __LINE__);
    (void) stream;

    mad_bit_finish(&stream->anc_ptr);
    mad_bit_finish(&stream->ptr);
}

/*
    NAME:	stream->buffer()
    DESCRIPTION:	set stream buffer pointers
*/
void mad_stream_buffer(struct mad_stream *stream,
                       unsigned char const *buffer, unsigned long length) {
    stack(__FUNCTION__, __FILE__, __LINE__);
    stream->buffer = buffer;
    stream->bufend = buffer + length;

    stream->this_frame = buffer;
    stream->next_frame = buffer;

    stream->sync = 1;

    mad_bit_init(&stream->ptr, buffer);
}

/*
    NAME:	stream->skip()
    DESCRIPTION:	arrange to skip bytes before the next frame
*/
void mad_stream_skip(struct mad_stream *stream, unsigned long length) {
    stack(__FUNCTION__, __FILE__, __LINE__);
    stream->skiplen += length;
}

/*
    NAME:	stream->sync()
    DESCRIPTION:	locate the next stream sync word
*/
int mad_stream_sync(struct mad_stream *stream) {
    register unsigned char const *ptr, *end;
    stack(__FUNCTION__, __FILE__, __LINE__);

    ptr = mad_bit_nextbyte(&stream->ptr);
    end = stream->bufend;

    while (ptr < end - 1 &&
            !(ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0)) {
        ++ptr;
    }

    if (end - ptr < MAD_BUFFER_GUARD) {
        return -1;
    }

    mad_bit_init(&stream->ptr, ptr);

    return 0;
}

/*
    NAME:	stream->errorstr()
    DESCRIPTION:	return a string description of the current error condition
*/
char const *mad_stream_errorstr(struct mad_stream const *stream) {
    stack(__FUNCTION__, __FILE__, __LINE__);
    switch (stream->error) {
    case MAD_ERROR_NONE:		 return PSTR("no error");

    case MAD_ERROR_BUFLEN:	 return PSTR("input buffer too small (or EOF)");
    case MAD_ERROR_BUFPTR:	 return PSTR("invalid (null) buffer pointer");

    case MAD_ERROR_NOMEM:		 return PSTR("not enough memory");

    case MAD_ERROR_LOSTSYNC:	 return PSTR("lost synchronization");
    case MAD_ERROR_BADLAYER:	 return PSTR("reserved header layer value");
    case MAD_ERROR_BADBITRATE:	 return PSTR("forbidden bitrate value");
    case MAD_ERROR_BADSAMPLERATE:	 return PSTR("reserved sample frequency value");
    case MAD_ERROR_BADEMPHASIS:	 return PSTR("reserved emphasis value");

    case MAD_ERROR_BADCRC:	 return PSTR("CRC check failed");
    case MAD_ERROR_BADBITALLOC:	 return PSTR("forbidden bit allocation value");
    case MAD_ERROR_BADSCALEFACTOR: return PSTR("bad scalefactor index");
    case MAD_ERROR_BADMODE:	 return PSTR("bad bitrate/mode combination");
    case MAD_ERROR_BADFRAMELEN:	 return PSTR("bad frame length");
    case MAD_ERROR_BADBIGVALUES:	 return PSTR("bad big_values count");
    case MAD_ERROR_BADBLOCKTYPE:	 return PSTR("reserved block_type");
    case MAD_ERROR_BADSCFSI:	 return PSTR("bad scalefactor selection info");
    case MAD_ERROR_BADDATAPTR:	 return PSTR("bad main_data_begin pointer");
    case MAD_ERROR_BADPART3LEN:	 return PSTR("bad audio data length");
    case MAD_ERROR_BADHUFFTABLE:	 return PSTR("bad Huffman table select");
    case MAD_ERROR_BADHUFFDATA:	 return PSTR("Huffman data overrun");
    case MAD_ERROR_BADSTEREO:	 return PSTR("incompatible block_type for JS");
    }

    return 0;
}
