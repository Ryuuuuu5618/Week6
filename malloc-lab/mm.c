/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12) // 힙 확장을 위한 기본 크기 (= 초기 빈 블록의 크기)

#define MAX(a, b) (a >= b ? a : b)

// 블록 사이즈, 할당 여부
#define PACK(size, alloc) ((size) | (alloc))
// 블록 조회 및 저장
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = val)

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/*
 * mm_init - initialize the malloc package.
 * 초기 힙 영역 할당 등 필요한 초기화를 수행 -> 초기화 과정에 문제가 있었다면 -1을, 그렇지 않으면 0을 반환
 */

static char *heap_listp;

void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);
void *extend_heap(size_t words);
void *coalesce(void *bp);
void *find(size_t asize);
void place(void *bp, size_t asize);

int mm_init(void)
{
    // 메모리 할당 실패
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    {
        return -1;
    }

    // 정렬 패딩
    PUT(heap_listp, 0);
    // 프롤로그 헤더
    PUT(heap_listp + WSIZE, PACK(8, 1));     // 헤더
    PUT(heap_listp + WSIZE * 2, PACK(8, 1)); // 푸터
    // 에필로그 헤더
    PUT(heap_listp + WSIZE * 3, PACK(0, 1));
    // heap_listp는 프롤로그 푸터의 시작점을 가리켜야 함
    heap_listp += WSIZE * 2;

    // 확장할 수 있는 힙 메모리 영역이 있는지
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 * 최소 size 바이트 크기의 할당된 블록 페이로드에 대한 포인터를 반환
 */
void *mm_malloc(size_t size)
{
    // 늘릴 사이즈 x -> 얼리 리턴
    if (size == 0)
    {
        return NULL;
    }

    // 가까운 8의 배수로 맞추기?
    size_t asize = (size <= DSIZE) ? DSIZE * 2 : DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);
    // 선언 및 첫 블록의 페이로드로 초기화
    // char *bp = heap_listp + DSIZE;
    char *bp;
    // 현재 힙 메모리에서 할당 가능한 가용 블록 찾기
    // while (GET_SIZE(HDRP(bp)) != 0)
    // {
    //     // 할당 가능
    //     if (GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp)))
    //     {
    //         if (GET_SIZE(HDRP(bp)) == asize)
    //         {
    //             PUT(HDRP(bp), PACK(asize, 1));
    //             PUT(FTRP(bp), PACK(asize, 1));
    //         }
    //         else
    //         {
    //             size_t free_size = GET_SIZE(HDRP(bp)) - asize;

    //             PUT(HDRP(bp), PACK(asize, 1));
    //             PUT(HDRP(bp) + asize - WSIZE, PACK(asize, 1));

    //             PUT(HDRP(bp) + asize, PACK(free_size, 0));
    //             PUT(FTRP(bp) + free_size, PACK(free_size, 0));

    //             coalesce(NEXT_BLKP(bp));
    //         }

    //         return bp;
    //     }

    //     bp = NEXT_BLKP(bp);
    // }
    if ((bp = (char *)find(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    // 힙 메모리 추가 할당
    if ((bp = extend_heap(MAX(asize, CHUNKSIZE) / WSIZE)) == NULL)
    {
        return NULL;
    }

    place(bp, asize);

    return bp;
}

// void *mm_malloc(size_t size)
// {
//     // 가까운 8의 배수로 맞추기?
//     int newsize = ALIGN(size + SIZE_T_SIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
//         return NULL;
//     else
//     {
//         // p가 헤더
//         *(size_t *)p = size;
//         // 페이로드의 시작점을 반환해야 하는 것 아닌가..?
//         // 헤더는 4바이트니까 (SIZE_T_SIZE / 2)를 해야..?
//         // return (void *)((char *)p + SIZE_T_SIZE);
//         // 페이로드 시작 주소 반환
//         return (void *)((char *)p + WSIZE);
//     }
// }

/*
 * mm_free - Freeing a block does nothing.
 * ptr이 가리키는 블록을 해제 + 반환값 x
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * mm_realloc 루틴은 다음 제약 조건에 따라 최소 size 바이트 크기의 할당된 영역에 대한 포인터를 반환합니다.
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    // 페이로드의 시작점 반환
    newptr = mm_malloc(size);

    if (newptr == NULL)
        return NULL;

    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = *(size_t *)((char *)oldptr - WSIZE);

    if (size < copySize)
        copySize = size;

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

void *extend_heap(size_t words)
{
    char *bp;

    // 워드 수 짝수로 맞추기~
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    // 항당받을 수 있는 힙 메모리가 있는지
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    // 연장된 힙 메모리 헤더 푸터 설정
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    // 새로운 에필로그 블록
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 힙 연장 전에 남아 있는 가용 블록이랑 합치기
    return coalesce(bp);
}
// 가용 블록 가짜 단편환 없애기
void *coalesce(void *bp)
{
    // 이전 블록과 다음 블록의 할당 여부 조사
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    // 현재 블록 사이즈
    size_t size = GET_SIZE(HDRP(bp));

    // 가용 블록 합칠지 판단하는 경우의 수 -> 4개
    // case 1. 둘 다 할당
    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    // case 2. 이전 블록 가용, 다음 블록 할당
    else if (!prev_alloc && next_alloc)
    {
        char *prev_bp_hd = HDRP(PREV_BLKP(bp));

        size += GET_SIZE(prev_bp_hd);

        PUT(prev_bp_hd, PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        bp = PREV_BLKP(bp);
    }
    // case 3. 이전 블록 할당, 다음 블록 가용
    else if (prev_alloc && !next_alloc)
    {
        char *next_bp_ft = FTRP(NEXT_BLKP(bp));

        size += GET_SIZE(next_bp_ft);

        PUT(HDRP(bp), PACK(size, 0));
        PUT(next_bp_ft, PACK(size, 0));
    }
    // case 4. 둘 다 가용
    else
    {
        char *prev_bp_hd = HDRP(PREV_BLKP(bp));
        char *next_bp_ft = FTRP(NEXT_BLKP(bp));

        size += (GET_SIZE(prev_bp_hd) + GET_SIZE(next_bp_ft));

        PUT(prev_bp_hd, PACK(size, 0));
        PUT(next_bp_ft, PACK(size, 0));

        bp = PREV_BLKP(bp);
    }

    return bp;
}
static void *last_bp = NULL;
void *find(size_t asize)
{
    // char *bp = (last_bp != NULL) ? last_bp : heap_listp + DSIZE;
    char *bp = heap_listp + DSIZE;
    while (GET_SIZE(HDRP(bp)) != 0)
    {
        // 할당 가능
        if (GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp)))
        {
            last_bp = bp;
            return bp;
        }

        bp = NEXT_BLKP(bp);
    }

    // bp = heap_listp + DSIZE;

    // while (bp != last_bp)
    // {
    //     // 할당 가능
    //     if (GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp)))
    //     {
    //         last_bp = bp;
    //         return bp;
    //     }

    //     bp = NEXT_BLKP(bp);
    // }

    return NULL;
}

void place(void *bp, size_t asize)
{
    if (GET_SIZE(HDRP(bp)) == asize)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    }
    else
    {
        size_t free_size = GET_SIZE(HDRP(bp)) - asize;

        PUT(HDRP(bp), PACK(asize, 1));
        PUT(HDRP(bp) + asize - WSIZE, PACK(asize, 1));

        PUT(HDRP(bp) + asize, PACK(free_size, 0));
        PUT(FTRP(bp) + free_size, PACK(free_size, 0));

        coalesce(NEXT_BLKP(bp));
    }
}