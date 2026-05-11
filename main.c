/*
 * Copyright (C) 2016 FIX94
 * Refactored 2026: WUX support + unified reader + improved output
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef _WIN32
#define __USE_LARGEFILE64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "platform.h"
#include "rijndael.h"
#include "sha1.h"
#include "fst.h"
#include "tmd.h"
#include "structs.h"
#include "reader.h"
#include "ui.h"

#define ALIGN_FORWARD(x, align) wud2app_align_forward((uint64_t)(x), (uint64_t)(align))

static const char *reader_type_str(reader_type_t t)
{
    switch (t) {
    case READER_WUX:   return "WUX";
    case READER_PARTS: return "PARTS";
    default:           return "WUD";
    }
}

int main(int argc, char *argv[])
{
    ui_header();

    char *ckeyChr = NULL, *gkeyChr = NULL, *srcPath = NULL;
    bool keys_in_folder = false;

    if (argc == 2) {
        srcPath = argv[1];
        keys_in_folder = true;
    } else if (argc == 4) {
        ckeyChr = argv[1];
        gkeyChr = argv[2];
        srcPath = argv[3];
    } else {
        printf(UI_STEP "Usage: wud2app <common.key> <game.key> <game.wud|wux>\n");
        printf(UI_STEP "       wud2app <wudump-folder>\n");
        return 0;
    }

    reader_t *r = reader_open(srcPath);
    if (!r) {
        printf(UI_ERR " Cannot open: %s\n", srcPath);
        return -1;
    }
    printf(UI_INFO "Source  %s  [%s]\n", srcPath, reader_type_str(reader_type(r)));

    /* Load common key */
    char keyPath[1024];
    if (keys_in_folder)
        snprintf(keyPath, sizeof(keyPath), "%s/common.key", srcPath);
    else
        snprintf(keyPath, sizeof(keyPath), "%s", ckeyChr);

    FILE *f = fopen(keyPath, "rb");
    if (!f) {
        printf(UI_ERR " Cannot open common.key: %s\n", keyPath);
        reader_close(r);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    if (ftell(f) != 16) {
        printf(UI_ERR " common.key must be exactly 16 bytes\n");
        fclose(f);
        reader_close(r);
        return -2;
    }
    rewind(f);
    uint8_t ckey[16];
    fread(ckey, 1, 16, f);
    fclose(f);

    /* Load game/disc key */
    if (keys_in_folder)
        snprintf(keyPath, sizeof(keyPath), "%s/game.key", srcPath);
    else
        snprintf(keyPath, sizeof(keyPath), "%s", gkeyChr);

    f = fopen(keyPath, "rb");
    if (!f) {
        printf(UI_ERR " Cannot open game.key: %s\n", keyPath);
        reader_close(r);
        return -3;
    }
    fseek(f, 0, SEEK_END);
    if (ftell(f) != 16) {
        printf(UI_ERR " game.key must be exactly 16 bytes\n");
        fclose(f);
        reader_close(r);
        return -4;
    }
    rewind(f);
    uint8_t gamekey[16];
    fread(gamekey, 1, 16, f);
    fclose(f);

    if (keys_in_folder)
        printf(UI_INFO "Keys    (from folder)\n\n");
    else
        printf(UI_INFO "Keys    %s  %s\n\n", ckeyChr, gkeyChr);

    /* Read disc name */
    reader_seek(r, 0);
    char outDir[11];
    outDir[10] = '\0';
    reader_read(r, outDir, 10);

    /* Decrypt partition table */
    uint8_t *partTblEnc = malloc(0x8000);
    reader_seek(r, 0x18000);
    reader_read(r, partTblEnc, 0x8000);

    uint8_t iv[16];
    memset(iv, 0, 16);
    aes_set_key(gamekey);
    uint8_t *partTbl = malloc(0x8000);
    aes_decrypt(iv, partTblEnc, partTbl, 0x8000);
    free(partTblEnc);

    uint32_t magic = wud2app_bswap32(*(uint32_t *)partTbl);
    if (magic != 0xCCA6E67B) {
        printf(UI_ERR " Invalid FST magic (wrong game key?)\n");
        goto extractEnd;
    }

    /* Validate TOC SHA1 */
    uint32_t expectedHash[5];
    for (int i = 0; i < 5; i++)
        expectedHash[i] = wud2app_bswap32(*(uint32_t *)(partTbl + 8 + i * 4));

    SHA1Context ctx;
    SHA1Reset(&ctx);
    SHA1Input(&ctx, partTbl + 0x800, 0x7800);
    SHA1Result(&ctx);

    if (memcmp(ctx.Message_Digest, expectedHash, 0x14) != 0) {
        printf(UI_ERR " FST TOC SHA1 mismatch\n");
        goto extractEnd;
    }
    printf(UI_INFO "FST validated" UI_OK "\n");

    int numPartitions = wud2app_bswap32(*(uint32_t *)(partTbl + 0x1C));
    toc_t *tbl = (toc_t *)(partTbl + 0x800);
    void *tmdBuf = NULL;
    bool certFound = false, tikFound = false, tmdFound = false;
    uint8_t tikKey[16];

    /* Find SI partition */
    int siPart;
    for (siPart = 0; siPart < numPartitions; siPart++) {
        if (wud2app_strncasecmp(tbl[siPart].name, "SI", 3) == 0)
            break;
    }
    if (wud2app_strncasecmp(tbl[siPart].name, "SI", 3) != 0) {
        printf(UI_ERR " SI Partition not found\n");
        goto extractEnd;
    }
    printf(UI_INFO "SI Partition found" UI_OK "\n");

    wud2app_mkdir(outDir);

    /* Read + decrypt SI FST */
    uint64_t offset = ((uint64_t)wud2app_bswap32(tbl[siPart].offsetBE)) * 0x8000 + 0x8000;
    void *fstEnc = malloc(0x8000);
    reader_seek(r, offset);
    reader_read(r, fstEnc, 0x8000);
    void *fstDec = malloc(0x8000);
    memset(iv, 0, 16);
    aes_set_key(gamekey);
    aes_decrypt(iv, fstEnc, fstDec, 0x8000);
    free(fstEnc);

    uint32_t EntryCount = wud2app_bswap32(*(uint32_t *)(fstDec + 8)) << 5;
    uint32_t Entries    = wud2app_bswap32(*(uint32_t *)(fstDec + 0x20 + EntryCount + 8));
    uint32_t NameOff    = 0x20 + EntryCount + (Entries << 4);
    FEntry *fe = (FEntry *)(fstDec + 0x20 + EntryCount);

    offset += 0x8000;
    uint32_t entry;
    for (entry = 1; entry < Entries; ++entry) {
        if (certFound && tikFound && tmdFound)
            break;
        uint32_t cNameOffset = wud2app_bswap32(fe[entry].NameOffset) >> 8;
        const char *name = (const char *)(fstDec + NameOff + cNameOffset);
        if (wud2app_strncasecmp(name, "title.", 6) != 0)
            continue;

        uint32_t CNTSize = wud2app_bswap32(fe[entry].FileLength);
        uint64_t CNTOff  = ((uint64_t)wud2app_bswap32(fe[entry].FileOffset)) << 5;
        uint64_t CNT_IV  = wud2app_bswap64(CNTOff >> 16);
        void *titleF = malloc(ALIGN_FORWARD(CNTSize, 16));
        reader_seek(r, offset + CNTOff);
        reader_read(r, titleF, ALIGN_FORWARD(CNTSize, 16));

        uint8_t *titleDec = malloc(ALIGN_FORWARD(CNTSize, 16));
        memset(iv, 0, 16);
        memcpy(iv + 8, &CNT_IV, 8);
        aes_set_key(gamekey);
        aes_decrypt(iv, titleF, titleDec, ALIGN_FORWARD(CNTSize, 16));
        free(titleF);

        char outF[64];
        snprintf(outF, sizeof(outF), "%s/%s", outDir, name);

        if (wud2app_strncasecmp(name, "title.cert", 11) == 0 && !certFound) {
            FILE *t = fopen(outF, "wb");
            fwrite(titleDec, 1, CNTSize, t);
            fclose(t);
            certFound = true;
            printf(UI_INFO "title.cert" UI_OK "\n");
        } else if (wud2app_strncasecmp(name, "title.tik", 10) == 0 && !tikFound) {
            uint32_t tidHigh = wud2app_bswap32(*(uint32_t *)(titleDec + 0x1DC));
            if (tidHigh == 0x00050000) {
                FILE *t = fopen(outF, "wb");
                fwrite(titleDec, 1, CNTSize, t);
                fclose(t);
                tikFound = true;
                printf(UI_INFO "title.tik" UI_OK "\n");

                uint8_t *title_id = titleDec + 0x1DC;
                for (int k = 0; k < 8; k++) {
                    iv[k]     = title_id[k];
                    iv[k + 8] = 0x00;
                }
                aes_set_key(ckey);
                aes_decrypt(iv, titleDec + 0x1BF, tikKey, 16);
            }
        } else if (wud2app_strncasecmp(name, "title.tmd", 10) == 0 && !tmdFound) {
            uint32_t tidHigh = wud2app_bswap32(*(uint32_t *)(titleDec + 0x18C));
            if (tidHigh == 0x00050000) {
                FILE *t = fopen(outF, "wb");
                fwrite(titleDec, 1, CNTSize, t);
                fclose(t);
                tmdFound = true;
                printf(UI_INFO "title.tmd" UI_OK "\n");
                tmdBuf = malloc(CNTSize);
                memcpy(tmdBuf, titleDec, CNTSize);
            }
        }
        free(titleDec);
    }
    free(fstDec);

    if (!tikFound || !tmdFound) {
        printf(UI_ERR " tik or tmd not found\n");
        goto extractEnd;
    }

    TitleMetaData *tmd = (TitleMetaData *)tmdBuf;
    char gmChar[19];
    uint64_t fullTid = wud2app_bswap64(tmd->TitleID);
    snprintf(gmChar, sizeof(gmChar), "GM%016" PRIx64, fullTid);
    printf("\n" UI_INFO "Game partition: %s\n\n", gmChar);

    int gmPart;
    for (gmPart = 0; gmPart < numPartitions; gmPart++) {
        if (wud2app_strncasecmp(tbl[gmPart].name, gmChar, 18) == 0)
            break;
    }
    if (wud2app_strncasecmp(tbl[gmPart].name, gmChar, 18) != 0) {
        printf(UI_ERR " GM Partition not found\n");
        goto extractEnd;
    }

    offset = ((uint64_t)wud2app_bswap32(tbl[gmPart].offsetBE)) * 0x8000;
    uint8_t *fHdr = malloc(0x8000);
    reader_seek(r, offset);
    reader_read(r, fHdr, 0x8000);
    uint32_t fHdrCnt = wud2app_bswap32(*(uint32_t *)(fHdr + 0x10));
    uint8_t *hashPos = fHdr + 0x40 + (fHdrCnt * 4);

    /* Read + write GM FST (.app 0) */
    uint64_t fstSize = wud2app_bswap64(tmd->Contents[0].Size);
    fstEnc = malloc(ALIGN_FORWARD(fstSize, 16));
    reader_seek(r, offset + 0x8000);
    reader_read(r, fstEnc, ALIGN_FORWARD(fstSize, 16));

    uint32_t fstContentCid = wud2app_bswap32(tmd->Contents[0].ID);
    char outF[64];
    snprintf(outF, sizeof(outF), "%s/%08x.app", outDir, fstContentCid);

    {
        char appName[32];
        snprintf(appName, sizeof(appName), "%08x.app", fstContentCid);
        ui_progress_done(appName, ALIGN_FORWARD(fstSize, 16));
        FILE *t = fopen(outF, "wb");
        fwrite(fstEnc, 1, ALIGN_FORWARD(fstSize, 16), t);
        fclose(t);
    }

    /* Decrypt FST for content table */
    memset(iv, 0, 16);
    uint16_t content_index = tmd->Contents[0].Index;
    memcpy(iv, &content_index, 2);
    aes_set_key(tikKey);
    fstDec = malloc(ALIGN_FORWARD(fstSize, 16));
    aes_decrypt(iv, fstEnc, fstDec, ALIGN_FORWARD(fstSize, 16));
    free(fstEnc);

    app_tbl_t *appTbl = (app_tbl_t *)(fstDec + 0x20);
    uint32_t appBufLen = 64 * 1024 * 1024;
    void *appBuf = malloc(appBufLen);

    uint16_t titleCnt = wud2app_bswap16(tmd->ContentCount);
    uint16_t curCont;
    for (curCont = 1; curCont < titleCnt; curCont++) {
        uint64_t appOffset      = ((uint64_t)wud2app_bswap32(appTbl[curCont].offsetBE)) * 0x8000;
        uint64_t totalAppOffset = offset + appOffset;
        reader_seek(r, totalAppOffset);

        uint64_t tSize         = wud2app_bswap64(tmd->Contents[curCont].Size);
        uint32_t curContentCid = wud2app_bswap32(tmd->Contents[curCont].ID);
        snprintf(outF, sizeof(outF), "%s/%08x.app", outDir, curContentCid);

        char appName[32];
        snprintf(appName, sizeof(appName), "%08x.app", curContentCid);

        FILE *t = fopen(outF, "wb");
        uint64_t total   = tSize;
        uint64_t written = 0;
        ui_progress(appName, 0, tSize);
        while (total > 0) {
            uint32_t toWrite = (total > (uint64_t)appBufLen)
                             ? appBufLen : (uint32_t)total;
            reader_read(r, appBuf, toWrite);
            fwrite(appBuf, 1, toWrite, t);
            total   -= toWrite;
            written += toWrite;
            ui_progress(appName, written, tSize);
        }
        fclose(t);
        ui_progress_done(appName, tSize);

        uint16_t type = wud2app_bswap16(tmd->Contents[curCont].Type);
        if (type & 2) {
            char h3Name[32];
            snprintf(h3Name, sizeof(h3Name), "%s/%08x.h3", outDir, curContentCid);
            uint32_t hashNum = (uint32_t)((tSize / 0x10000000ULL) + 1);
            FILE *h = fopen(h3Name, "wb");
            fwrite(hashPos, 1, 0x14 * hashNum, h);
            fclose(h);
            hashPos += 0x14 * hashNum;
        }
    }

    free(fstDec);
    free(appBuf);
    free(tmdBuf);
    free(fHdr);
    printf("\n" C_BOLD C_GREEN "  Done!" C_RESET "  Output: %s/\n", outDir);

extractEnd:
    free(partTbl);
    reader_close(r);
    return 0;
}
