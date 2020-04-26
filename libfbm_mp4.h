#include <mp4v2/mp4v2.h>

#ifndef LIBFBM_MP4_H
#define LIBFBM_MP4_H

#define BUFFER_SIZE         (1920*1080)
#define FRAME_FRATE         (15)
#define TIME_SCALE          (90000)

typedef struct _MP4ENC_NaluUnit {
    int type;
    int size;
    unsigned char *data;
} MP4ENC_NaluUnit;

int muxer_mp4(const char*, const char*, const char*);
int _muxer_mp4(const char* h264, int width, int height, const char* aac, int sr, const char* mp4);

#endif /* LIBFBM_MP4_H */
