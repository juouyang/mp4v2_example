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
#include <pthread.h>

#include "libfbm_mp4.h"

static int running = 3;

static void* thread_libfbm_video_0(void* arg) {
    printf("thread_libfbm_video_0\n");
    do {
        int ret = _muxer_mp4("/media/sf_Downloads/foo/foo.h264"
                , 1920
                , 1080
                , "/media/sf_Downloads/foo/foo.aac"
                , 48000
                , "/media/sf_Downloads/foo/foo-mp4v2.mp4");
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
    int ret = 0;
    pthread_t ThreadVideoFrameData_ID_1, ThreadVideoFrameData_ID_2, ThreadVideoFrameData_ID_3;

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
