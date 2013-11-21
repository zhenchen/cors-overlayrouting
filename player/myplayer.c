#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>

#include <SDL.h>
#include <SDL_thread.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLEN 2048
#define RTP_HEADER_LEN 12

int InitSocket(const struct sockaddr_in *local) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (bind(sock, (struct sockaddr *)local, sizeof(struct sockaddr)) < 0) {
    fprintf(stderr, "binding socket error\n");
    close(sock);
    return -1;
  }
  return sock;
}

struct block_t {
  uint8_t *data;
  int size;
  int mark;
  int seq;
  int ts;
  struct block_t *prev;
  struct block_t *next;
};

struct blockQueue {
  struct block_t *first;
  struct block_t *last;
  int size_in_queue;
};

struct block_t *ParseRTP(uint8_t *rawdata, int rawlen) {
  int rtpVersion;
  int CSRCCount;
  int mark;
  int payloadType;
  int skip = 0;
  int extensionFlag = 0;
  int extensionLength = 0;
  uint16_t sequenceNumber = 0;
  uint32_t timestamp = 0;

  if (rawlen < RTP_HEADER_LEN)
    return NULL;

  rtpVersion = (rawdata[0] & 0xC0) >> 6;
  CSRCCount = rawdata[0] & 0x0F;
  extensionFlag = rawdata[0] & 0x10;
  mark = rawdata[1] & 0x80;
  payloadType = rawdata[1] & 0x7F;
  sequenceNumber = (rawdata[2] << 8) + rawdata[3];
  timestamp = (rawdata[4] << 24) + (rawdata[5] << 16) + (rawdata[6] << 8) + rawdata[7];

  if (extensionFlag) 
    extensionLength = 4 + 4 * ((rawdata[14] << 8) + rawdata[15]);

  skip = RTP_HEADER_LEN + 4 * CSRCCount + extensionLength;
  if (skip >= rawlen)
    return NULL;

  struct block_t *pBlock = (struct block_t *)av_malloc(sizeof(struct block_t));
  pBlock->data = (uint8_t *)av_malloc((rawlen - skip) * sizeof(uint8_t));
  memcpy(pBlock->data, rawdata + skip, rawlen - skip);
  pBlock->size = rawlen - skip;
  pBlock->mark = mark;
  pBlock->seq = sequenceNumber;
  pBlock->ts = timestamp;
  pBlock->prev = pBlock->next = NULL;

  return pBlock;
}

struct block_t *JoinRTP(struct blockQueue *pQueue, struct block_t *pBlock, int *finished) {
  if (pQueue->first == NULL && pBlock == NULL) {
    *finished = 0;
    return NULL;
  }

  if (pQueue->first == NULL) {
    pQueue->first = pBlock;
    pQueue->last = pBlock;
 
    if (pBlock->mark)
      pQueue->size_in_queue = 4 + pBlock->size; /* 00 00 00 01 + pBlock */
    else
      pQueue->size_in_queue = 3 + pBlock->size; /* 00 00 01 + pBlock */

    *finished = 0;
    return NULL;
  }
  
  int lastTimestamp = pQueue->last->ts;
  if (pBlock != NULL && pBlock->ts == lastTimestamp) {
    pQueue->last->next = pBlock;
    pBlock->prev = pQueue->last;
    pQueue->last = pBlock;
    
    if (pBlock->mark) 
      pQueue->size_in_queue += (4 + pBlock->size);
    else
      pQueue->size_in_queue += (3 + pBlock->size);

    *finished = 0;
    return NULL;
  } 

  struct block_t *pRetBlk = (struct block_t *)av_malloc(sizeof(struct block_t));
  pRetBlk->data = (uint8_t *)av_malloc(pQueue->size_in_queue * sizeof(uint8_t));
  pRetBlk->size = pQueue->size_in_queue;
  pRetBlk->ts = lastTimestamp;

  struct block_t *pCurr = pQueue->first;
  int offset = 0;
  while (pCurr != NULL) {
    if (pCurr->mark) {
      memset(pRetBlk->data + offset, 0x0, 3);
      memset(pRetBlk->data + offset + 3, 0x1, 1);
      memcpy(pRetBlk->data + offset + 4, pCurr->data, pCurr->size);
      offset += (4 + pCurr->size);
    } else {
      memset(pRetBlk->data + offset, 0x0, 2);
      memset(pRetBlk->data + offset + 2, 0x1, 1); 
      memcpy(pRetBlk->data + offset + 3, pCurr->data, pCurr->size);
      offset += (3 + pCurr->size);
    }
    pQueue->first = pCurr->next;
    if (pQueue->first != NULL) {
      pQueue->first->prev = NULL;
    }
    av_free(pCurr->data);
    av_free(pCurr);
    pCurr = pQueue->first;
  }
  pQueue->last = NULL;

  if (pBlock != NULL) {
    pQueue->first = pBlock;
    pQueue->last = pBlock;

    if (pBlock->mark)
      pQueue->size_in_queue = 4 + pBlock->size; /* 00 00 00 01 + pBlock */
    else
      pQueue->size_in_queue = 3 + pBlock->size; /* 00 00 01 + pBlock */
  }

  *finished = 1;
  return pRetBlk;
}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int y;

  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile = fopen(szFilename, "wb");
  if (pFile == NULL)
    return;

  fprintf(pFile, "P6\n%d %d\n255\n", width, height);
  for (y = 0; y < height; y++) {
    fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width*3, pFile);
  }
  fclose(pFile);
}

int main(int argc, char *argv[]) {
  int ret = 0;

  struct sockaddr_in local;
  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_port = htons(5000);
  local.sin_addr.s_addr = htonl(INADDR_ANY);

  int sock = InitSocket(&local);
  if (sock < 0) 
    return -1;

  av_register_all();

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    return -1;
  }

  AVCodecContext *pCodecCtx = avcodec_alloc_context();

  AVCodec *pCodec = avcodec_find_decoder(CODEC_ID_H264);
  if (pCodec == NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    ret = -1;
    goto error;
  }

  if (avcodec_open(pCodecCtx, pCodec) < 0) {
    fprintf(stderr, "avcodec_open() failed\n");
    ret = -1;
    goto error;
  }
  fprintf(stdout, "avcodec_open() success\n");

  AVFrame *pFrame = avcodec_alloc_frame();
  if (pFrame == NULL) {
    fprintf(stderr, "avcodec_alloc_frame() error\n");
    ret = -1;
    goto error;
  }

  SDL_Surface *screen;

#ifndef __DARWIN__
  screen = SDL_SetVideoMode(352, 288, 0, 0);
#else
  screen = SDL_SetVideoMode(352, 288, 24, 0);
#endif
  if (!screen) {
    fprintf(stderr, "SDL: could not set video mode - exiting\n");
    ret = -1;
    goto error;
  }

  SDL_Overlay *bmp = SDL_CreateYUVOverlay(352, 288, SDL_YV12_OVERLAY, screen);

  struct blockQueue queue;
  queue.first = queue.last = NULL;
  queue.size_in_queue = 0;

  /*fdump = fopen("dump.264", "wb");*/

  fd_set rfds;
  struct sockaddr_in from;
  socklen_t fromlen = sizeof(struct sockaddr_in);
  uint8_t rbuf[MAXLEN];
  SDL_Rect rect;
  SDL_Event event;
  struct block_t *pBlock, *pJoinBlock;

  while (1) {
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    int selectStatus = select(sock + 1, &rfds, NULL, NULL, NULL);
    if (selectStatus < 0) {
      fprintf(stderr, "select() error\n");
      ret = -1;
      goto error;
    } else if (selectStatus == 0) {
      break;
    }
    
    int rlen = recvfrom(sock, rbuf, MAXLEN, 0, (struct sockaddr *)&from, &fromlen);
    pBlock = ParseRTP(rbuf, rlen);
    if (pBlock == NULL)
      continue;
    
    int joinFinished = 0;
    pJoinBlock = JoinRTP(&queue, pBlock, &joinFinished);
    if (!joinFinished) {
      continue;
    }
    /*fwrite(pJoinBlock->data, 1, pJoinBlock->size, fdump);*/

    int frameFinished = 0;
    avcodec_decode_video(pCodecCtx, pFrame, &frameFinished, pJoinBlock->data, pJoinBlock->size);
    if (frameFinished) {
      SDL_LockYUVOverlay(bmp);
      AVPicture pict;
      pict.data[0] = bmp->pixels[0];
      pict.data[1] = bmp->pixels[2];
      pict.data[2] = bmp->pixels[1];

      pict.linesize[0] = bmp->pitches[0];
      pict.linesize[1] = bmp->pitches[2];
      pict.linesize[2] = bmp->pitches[1];
      
      img_convert(&pict, PIX_FMT_YUV420P,
                  (AVPicture *)pFrame, pCodecCtx->pix_fmt,
                  352, 288);
      
      SDL_UnlockYUVOverlay(bmp);

      rect.x = 0;
      rect.y = 0;
      rect.w = 352;
      rect.h = 288;
      SDL_DisplayYUVOverlay(bmp, &rect);
    }
    av_free(pJoinBlock->data);
    av_free(pJoinBlock);
    SDL_PollEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        SDL_Quit();
        exit(0);
        break;
      default:
        break;
    }
  }
  int joinFinished = 0;
  pJoinBlock = JoinRTP(&queue, NULL, &joinFinished);
  if (joinFinished) {
    /*fwrite(pJoinBlock->data, 1, pJoinBlock->size, fdump);*/
    av_free(pJoinBlock->data);
    av_free(pJoinBlock);
  }

  /*fclose(fdump);*/
  av_free(pFrame);
  avcodec_close(pCodecCtx);
  ret = 0;

error:
  close(sock);
  return ret;
}
