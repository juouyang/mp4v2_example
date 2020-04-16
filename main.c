/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: ju
 *
 * Created on April 14, 2020, 11:34 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <mp4v2/mp4v2.h>

#define BUFFER_SIZE         (1920*1080)
#define FRAME_FRATE         (15)
#define TIME_SCALE          (90000)

typedef struct _MP4ENC_NaluUnit {
    int type;
    int size;
    unsigned char *data;
} MP4ENC_NaluUnit;

static inline int _muxer_mp4(const char* h264, int width, int height, const char* aac, int sr, const char* mp4);

static inline unsigned long GetTickCount();
static inline int _read_aac(FILE* m_fp_AAC, unsigned char* buf, int buf_size);
static inline int _read_h264(FILE* m_fp_h264, unsigned char* buf, int buf_size);
static inline int _read_one_nalu_from_buf(const unsigned char* buffer, unsigned int nBufferSize, unsigned int offSet, MP4ENC_NaluUnit* nalu);
static inline int _write_h264(int* m_videoId, int* sps_wt, int* pps_wt, MP4FileHandle hMp4File, int w, int h, const unsigned char* pData, int size);

static int running = 3;

static void* thread_libfbm_video_0(void* arg) {
    printf("thread_libfbm_video_0\n");
    do {
        int ret = _muxer_mp4("/media/sf_Downloads/foo/foo.h264"
                , 1920
                , 1080
                , "/media/sf_Downloads/foo/foo.aac"
                , 48000
                , "/media/sf_Downloads/foo/foo.mp4");
        printf("ret = %d\n", ret);
    } while (0);
    running--;
    pthread_exit(NULL);
}

static void* thread_libfbm_video_1(void* arg) {
    printf("thread_libfbm_video_1\n");
    do {
        int ret = _muxer_mp4("/media/sf_Downloads/foo/foo-1.h264"
                , 1920
                , 1080
                , "/media/sf_Downloads/foo/foo-1.aac"
                , 48000
                , "/media/sf_Downloads/foo/foo-1.mp4");
        printf("ret = %d\n", ret);
    } while (0);
    running--;
    pthread_exit(NULL);
}

static void* thread_libfbm_video_2(void* arg) {
    printf("thread_libfbm_video_2\n");
    do {
        int ret = _muxer_mp4("/media/sf_Downloads/foo/foo-2.h264"
                , 1920
                , 1080
                , "/media/sf_Downloads/foo/foo-2.aac"
                , 48000
                , "/media/sf_Downloads/foo/foo-2.mp4");
        printf("ret = %d\n", ret);
    } while (0);
    running--;
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    pthread_t ThreadVideoFrameData_ID_1, ThreadVideoFrameData_ID_2, ThreadVideoFrameData_ID_3;
    int ret = 0;
    if ((ret = pthread_create(&ThreadVideoFrameData_ID_1, NULL, &thread_libfbm_video_0, (void *) NULL))) {
        printf("Failed to create libfbm video thread 0, ret=[%d]", ret);
        return -1;
    }
    pthread_detach(ThreadVideoFrameData_ID_1);

    if ((ret = pthread_create(&ThreadVideoFrameData_ID_2, NULL, &thread_libfbm_video_1, (void *) NULL))) {
        printf("Failed to create libfbm video thread 1, ret=[%d]", ret);
        return -1;
    }
    pthread_detach(ThreadVideoFrameData_ID_2);

    if ((ret = pthread_create(&ThreadVideoFrameData_ID_3, NULL, &thread_libfbm_video_2, (void *) NULL))) {
        printf("Failed to create libfbm video thread 2, ret=[%d]", ret);
        return -1;
    }
    pthread_detach(ThreadVideoFrameData_ID_3);

    while (running > 0) {
        sleep(1);
    }

    return 0;
}

static inline unsigned long GetTickCount() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((double) ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0);
}

static inline int _read_aac(FILE* m_fp_AAC, unsigned char* buf, int buf_size) {
    unsigned char aac_header[7];
    int true_size = 0;

    true_size = fread(aac_header, 1, 7, m_fp_AAC);
    if (true_size <= 0) {
        return -2;
        fseek(m_fp_AAC, 0L, SEEK_SET);
        return _read_aac(m_fp_AAC, buf, buf_size);
    } else {
        unsigned int body_size = *(unsigned int *) (aac_header + 3);
        body_size = ntohl(body_size); //Little Endian
        body_size = body_size << 6;
        body_size = body_size >> 19;

        true_size = fread(buf, 1, body_size - 7, m_fp_AAC);

        return true_size;
    }
}

static inline int _read_h264(FILE* m_fp_h264, unsigned char* buf, int buf_size) {
    int true_size = fread(buf, 1, buf_size, m_fp_h264);
    if (true_size > 0) {
        return true_size;
    } else {
        return 0;
    }
}

static inline int _read_one_nalu_from_buf(const unsigned char* buffer, unsigned int nBufferSize, unsigned int offSet, MP4ENC_NaluUnit* nalu) {
    int i = offSet;
    while (i < nBufferSize) {
        if (buffer[i++] == 0x00 &&
                buffer[i++] == 0x00 &&
                buffer[i++] == 0x00 &&
                buffer[i++] == 0x01
                ) {
            int pos = i;
            while (pos < nBufferSize) {
                if (buffer[pos++] == 0x00 &&
                        buffer[pos++] == 0x00 &&
                        buffer[pos++] == 0x00 &&
                        buffer[pos++] == 0x01
                        ) {
                    break;
                }
            }
            if (pos == nBufferSize) {
                nalu->size = pos - i;
            } else {
                nalu->size = (pos - 4) - i;
            }

            nalu->type = buffer[i] & 0x1f;
            nalu->data = (unsigned char*) &buffer[i];
            return (nalu->size + i - offSet);
        }
    }
    return 0;
}

static inline int _write_h264(int* m_videoId, int* sps_wt, int* pps_wt, MP4FileHandle hMp4File, int w, int h, const unsigned char* pData, int size) {
    if (hMp4File == NULL) {
        return -1;
    }
    if (pData == NULL) {
        return -1;
    }

    MP4ENC_NaluUnit nalu = {0};
    int pos = 0;
    int len = 0;
    int wt_frame = 0;

    while (len = _read_one_nalu_from_buf(pData, size, pos, &nalu)) {
        if (nalu.type == 0x07 && *sps_wt == 0) { // sps
            *m_videoId = MP4AddH264VideoTrack
                    (hMp4File,
                    TIME_SCALE,
                    (double) TIME_SCALE / FRAME_FRATE,
                    w, // width
                    h, // height
                    nalu.data[1], // sps[1] AVCProfileIndication
                    nalu.data[2], // sps[2] profile_compat
                    nalu.data[3], // sps[3] AVCLevelIndication
                    3); // 4 bytes length before each NAL unit
            if (*m_videoId == MP4_INVALID_TRACK_ID) {
                printf("add video track failed.\n");
                return 0;
            }
            MP4SetVideoProfileLevel(hMp4File, 1); //  Simple Profile @ Level 3
            MP4AddH264SequenceParameterSet(hMp4File, *m_videoId, nalu.data, nalu.size);
            *sps_wt = 1;
        } else if (nalu.type == 0x08 && *pps_wt == 0) { // pps
            MP4AddH264PictureParameterSet(hMp4File, *m_videoId, nalu.data, nalu.size);
            *pps_wt = 1;
        } else if (nalu.type == 0x01 || nalu.type == 0x05) {
            int datalen = nalu.size + 4;
            unsigned char* data = malloc(datalen);
            data[0] = nalu.size >> 24;
            data[1] = nalu.size >> 16;
            data[2] = nalu.size >> 8;
            data[3] = nalu.size & 0xff;
            memcpy(data + 4, nalu.data, nalu.size);

            bool syn = 0;
            if (nalu.type == 0x05) {
                syn = 1;
            }
            if (!MP4WriteSample(hMp4File, *m_videoId, data, datalen, MP4_INVALID_DURATION, 0, syn)) {
                return 0;
            }
            if (data) {
                free(data);
                data = NULL;
            }
            wt_frame++;
        }

        pos += len;
        if (wt_frame > 0) {
            break;
        }
    }
    return pos;
}

static inline int _muxer_mp4(const char* h264, int width, int height, const char* aac, int sr, const char* mp4) {

    int ret = 0;
    FILE* m_fp_h264 = NULL;
    FILE* m_fp_AAC = NULL;
    MP4FileHandle m_hMp4File = NULL;

    do {
        //
        double audio_tick_gap = (1000.0) / 60;
        double video_tick_gap = (1000.0) / FRAME_FRATE;
        //--------------------------------------------------------------------
        m_fp_h264 = fopen(h264, "rb");
        if (!m_fp_h264) {
            ret = -1;
            break;
        }
        m_fp_AAC = fopen(aac, "rb");
        if (!m_fp_AAC) {
            ret = -1;
            break;
        }
        m_hMp4File = MP4Create(mp4, 0);
        if (m_hMp4File == MP4_INVALID_FILE_HANDLE) {
            printf("ERROR:Open file fialed.\n");
            ret = -1;
            break;
        }
        //--------------------------------------------------------------------
        MP4SetTimeScale(m_hMp4File, TIME_SCALE);
        MP4TrackId m_audioId = MP4AddAudioTrack(m_hMp4File, sr, 1024, MP4_MPEG4_AUDIO_TYPE); // 1024?
        if (m_audioId == MP4_INVALID_TRACK_ID) {
            printf("add audio track failed.\n");
            ret = -1;
            break;
        }
        MP4SetAudioProfileLevel(m_hMp4File, 0x2);
        /*
         * The parameter: pConfig of MP4SetTrackESConfiguration
         * 
         * Very Important! Expect AAC 48K mono
         * MUST change pConfig MANNUALLY if the input AAC is NOT 48K mono 
         * MP4 will have NO sound if pConfig is NOT set according to the input audio
         * 
         * 5 bits | 4 bits | 4 bits | 3 bits
         * 第一欄    第二欄    第三欄    第四欄
         * 
         * 第一欄：AAC Object Type
         * 第二欄：Sample Rate Index
         * 第三欄：Channel Number
         * 第四欄：Don't care，設 0
         * 
            int GetSampleRateIndex(unsigned int sampleRate)
            {
                if (92017 <= sampleRate) return 0;
                if (75132 <= sampleRate) return 1;
                if (55426 <= sampleRate) return 2;
                if (46009 <= sampleRate) return 3;  // 48K
                if (37566 <= sampleRate) return 4;
                if (27713 <= sampleRate) return 5;
                if (23004 <= sampleRate) return 6;
                if (18783 <= sampleRate) return 7;
                if (13856 <= sampleRate) return 8;  // 16K
                if (11502 <= sampleRate) return 9;
                if (9391 <= sampleRate) return 10;
                return 11;
            }
         * 
         * Example 1: AAC 16K Mono
         *   第一欄：00010
         *   第二欄：1000
         *   第三欄：0001
         *   第四欄：000
         * 
         *   00010100, 00001000
         *   0x14    , 0x08
         * 
         * Example 2: AAC 48K Mono
         *   第一欄：00010
         *   第二欄：0011
         *   第三欄：0001
         *   第四欄：000
         * 
         *   00010001, 10001000
         *   0x11    , 0x88
         */
        uint8_t buf3[2] = {0x0, 0x0};
        if (sr == 48000) {
            buf3[0] = 0x11;
            buf3[1] = 0x88;
        } else if (sr == 16000) {
            buf3[0] = 0x14;
            buf3[1] = 0x08;
        }
        MP4SetTrackESConfiguration(m_hMp4File, m_audioId, buf3, 2);
        //--------------------------------------------------------------------
        unsigned char buffer[BUFFER_SIZE];
        unsigned char audioBuf[1024];
        int pos = 0;
        int readlen = 0;
        int writelen = 0;
        //--------------------------------------------------------------------
        unsigned long long audio_tick_now = 0;
        unsigned long long video_tick_now = 0;
        unsigned long long last_update = 0;
        unsigned long long audio_tick = 0;
        unsigned long long video_tick = 0;
        unsigned long long tick_exp_new = 0;
        unsigned long long tick_exp = 0;
        //--------------------------------------------------------------------
        audio_tick_now = video_tick_now = GetTickCount();

        MP4TrackId m_videoId = -1;
        int sps_wt = 0;
        int pps_wt = 0;
        while (1) {
            last_update = GetTickCount();

            if (last_update - audio_tick_now > audio_tick_gap - tick_exp) {
                // printf("now:%lld last_update:%lld audio_tick:%lld tick_exp:%lld\n", audio_tick_now, last_update, audio_tick, tick_exp);
                audio_tick += audio_tick_gap;
                int audio_len = _read_aac(m_fp_AAC, audioBuf, 1024);
                if (audio_len == -2) {
                    goto h264;
                } else if (audio_len <= 0) {
                    break;
                }

                MP4WriteSample(m_hMp4File, m_audioId, audioBuf, audio_len, MP4_INVALID_DURATION, 0, 1);
                audio_tick_now = GetTickCount();
            }
h264:
            if (last_update - video_tick_now > video_tick_gap - tick_exp) {
                // printf("now:%lld last_update:%lld video_tick:%lld tick_exp:%lld\n", video_tick_now, last_update, video_tick, tick_exp);
                video_tick += video_tick_gap;
                readlen = _read_h264(m_fp_h264, buffer + pos, BUFFER_SIZE - pos);
                if (readlen <= 0 && pos == 0) {
                    break;
                }
                readlen += pos;
                writelen = 0;
                for (int i = readlen; i >= 4; i--) {
                    if (buffer[i - 1] == 0x01 &&
                            buffer[i - 2] == 0x00 &&
                            buffer[i - 3] == 0x00 &&
                            buffer[i - 4] == 0x00
                            ) {
                        writelen = i - 4;
                        break;
                    }
                }
                writelen = _write_h264(&m_videoId, &sps_wt, &pps_wt, m_hMp4File, width, height, buffer, writelen);
                if (writelen <= 0) {
                    break;
                }
                memcpy(buffer, buffer + writelen, readlen - writelen);
                pos = readlen - writelen;
                if (pos == 0) {
                    break;
                }
                video_tick_now = GetTickCount();
            }

            tick_exp_new = GetTickCount();
            tick_exp = tick_exp_new - last_update;
        } // while(1)
        ret = 0;
    } while (0);

    if (m_hMp4File != NULL) {
        MP4Close(m_hMp4File, 0);
        m_hMp4File = NULL;
    }
    if (m_fp_h264 != NULL) {
        fclose(m_fp_h264);
        m_fp_h264 = NULL;
    }
    if (m_fp_AAC != NULL) {
        fclose(m_fp_AAC);
        m_fp_AAC = NULL;
    }

    return ret;
}
