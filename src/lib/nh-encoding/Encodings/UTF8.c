// LICENSE NOTICE ==================================================================================

/**
 * Netzhaut - Web Browser Engine
 * Copyright (C) 2022  Dajo Frey
 * Published under GNU LGPL. See Netzhaut/LICENSE.LGPL file.
 */

// INCLUDES ========================================================================================

#include "UTF8.h"
#include "UTF32.h"

#include "../../nh-core/Util/String.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DECLARE =========================================================================================

typedef struct nh_encoding_UTF8Decoder {
    NH_ENCODING_UTF32 codepoint;
    unsigned char bytesSeen;
    unsigned char bytesNeeded;
    unsigned char lowerBoundary;
    unsigned char upperBoundary;
} nh_encoding_UTF8Decoder;

// DECODER =========================================================================================

static nh_encoding_UTF8Decoder nh_encoding_initUTF8Decoder()
{
    nh_encoding_UTF8Decoder Decoder;
    
    Decoder.codepoint     = 0;
    Decoder.bytesSeen     = 0;
    Decoder.bytesNeeded   = 0;
    Decoder.lowerBoundary = 0x80;
    Decoder.upperBoundary = 0xBF;

    return Decoder;
}

// https://encoding.spec.whatwg.org/#utf-8-encoder
static int nh_encoding_decodeUTF8Codepoint(
    nh_encoding_UTF8Decoder *Decoder_p, unsigned char byte) 
{
    if (Decoder_p->bytesNeeded == 0) 
    {
        if (byte <= 0x7F) {
            Decoder_p->codepoint = byte;
            return 1;
        }
        else if (byte >= 0xC2 && byte <= 0xDF) {
            Decoder_p->bytesNeeded = 1;
            Decoder_p->codepoint   = byte & 0x1F;
        }
        else if (byte >= 0xE0 && byte <= 0xEF) {
            if (byte == 0xE0) {Decoder_p->lowerBoundary = 0xA0;}
            if (byte == 0xED) {Decoder_p->upperBoundary = 0x9F;}
            Decoder_p->bytesNeeded = 2;
            Decoder_p->codepoint   = byte & 0xF;
        }
        else if (byte >= 0xF0 && byte <= 0xF4) {
            if (byte == 0xF0) {Decoder_p->lowerBoundary = 0x90;}
            if (byte == 0xF4) {Decoder_p->upperBoundary = 0x8F;}
            Decoder_p->bytesNeeded = 3;
            Decoder_p->codepoint   = byte & 0x7;
        }
        else {
            return 2;
        }

        return 0;
    }

    if (byte < Decoder_p->lowerBoundary || byte > Decoder_p->upperBoundary) 
    {
        *Decoder_p = nh_encoding_initUTF8Decoder();
        return 2;
    }

    Decoder_p->lowerBoundary = 0x80;
    Decoder_p->upperBoundary = 0xBF;
    Decoder_p->codepoint = (Decoder_p->codepoint << 6) | (byte & 0x3F);
    Decoder_p->bytesSeen++;

    if (Decoder_p->bytesSeen != Decoder_p->bytesNeeded) {
        return 0;
    }
    
    return 1;
}

nh_encoding_UTF32String nh_encoding_decodeUTF8(
    unsigned char *p, unsigned long bytes, unsigned long *read_p)
{
    nh_encoding_UTF32String Result = nh_encoding_initUTF32(64);
    nh_encoding_UTF8Decoder Decoder = nh_encoding_initUTF8Decoder();

    for (unsigned long i = 0; i < bytes; ++i) 
    {
        int result = nh_encoding_decodeUTF8Codepoint(&Decoder, p[i]);

        if (result)
        {
            if (result == 2) {break;} // error

            nh_encoding_appendUTF32(&Result, &Decoder.codepoint, 1);
            Decoder.codepoint   = 0;
            Decoder.bytesNeeded = 0;
            Decoder.bytesSeen   = 0;

            if (read_p) {*read_p = i + 1;}
        }
    }

    return Result;
}

NH_ENCODING_UTF32 nh_encoding_decodeUTF8Single(
    unsigned char *p, unsigned long bytes, unsigned long *read_p)
{
    nh_encoding_UTF8Decoder Decoder = nh_encoding_initUTF8Decoder();
    NH_ENCODING_UTF32 codepoint = 0;

    for (unsigned long i = 0; i < bytes; ++i) {
        int result = nh_encoding_decodeUTF8Codepoint(&Decoder, p[i]);
        if (result) {
            if (result == 2) {break;} // error
            codepoint = Decoder.codepoint;
            if (read_p) {*read_p = 1;}
            break;
        }
    }

    return codepoint;
}

// ENCODER =========================================================================================

// https://encoding.spec.whatwg.org/#utf-8-encoder
NH_API_RESULT nh_encoding_appendUTF8(
    nh_encoding_UTF8String *String_p, NH_ENCODING_UTF32 *codepoints_p, unsigned long length)
{
    for (unsigned long i = 0; i < length; ++i) 
    {
        if (nh_encoding_isASCIICodepoint(codepoints_p[i])) {
            char byte = codepoints_p[i];
            nh_core_appendToString(String_p, &byte, 1);
            continue;
        }

        int count, offset;

        if (codepoints_p[i] >= 0x0080 && codepoints_p[i] <= 0x07FF) {
            count = 1;
            offset = 0xC0;
        }
        if (codepoints_p[i] >= 0x0800 && codepoints_p[i] <= 0xFFFF) {
            count = 2;
            offset = 0xE0;
        }
        if (codepoints_p[i] >= 0x10000 && codepoints_p[i] <= 0x10FFFF) {
            count = 3;
            offset = 0xF0;
        }

        char p[4];
        memset(p, 0, 4);
        p[0] = (codepoints_p[i] >> (6 * count)) + offset;
        int index = 1, bytes = count + 1;

        while (count) {
            char temp = codepoints_p[i] >> (6 * (count - 1));
            p[index++] = 0x80 | (temp & 0x3F);
            count--;
        }

        nh_core_appendToString(String_p, p, bytes);
    }

    return NH_API_SUCCESS;
}

NH_API_RESULT nh_encoding_appendUTF8Single(
    nh_encoding_UTF8String *String_p, NH_ENCODING_UTF32 codepoint)
{
    NH_CORE_CHECK(nh_encoding_appendUTF8(String_p, &codepoint, 1))

    return NH_API_SUCCESS;
}

nh_encoding_UTF8String nh_encoding_encodeUTF8(
    NH_ENCODING_UTF32 *codepoints_p, unsigned long length)
{
    nh_encoding_UTF8String Result = nh_core_initString(64);
    nh_encoding_appendUTF8(&Result, codepoints_p, length);

    return Result;
}

int nh_encoding_encodeUTF8Single(
    NH_ENCODING_UTF32 codepoint, char *p)
{
    nh_encoding_UTF8String String = nh_encoding_encodeUTF8(&codepoint, 1);
    int count = String.length;
    if (p) {for (int i = 0; i < String.length; ++i) {p[i] = String.p[i];}}
    nh_core_freeString(&String);

    return count;
}

// INIT/FREE =======================================================================================

nh_encoding_UTF8String nh_encoding_initUTF8(
    int length)
{
    return nh_core_initString(length);
}

void nh_encoding_freeUTF8(
    nh_encoding_UTF8String *String_p)
{
    nh_core_freeString(String_p);
    return;
}

