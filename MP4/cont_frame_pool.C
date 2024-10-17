/*
 File: ContFramePool.C
 
 Author: Caleb Elizondo
 Date  : 9/15/2023
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/


const unsigned int ContFramePool::maxPools = 2;
unsigned int ContFramePool::npools = 0;
ContFramePool* ContFramePool::pools[ContFramePool::maxPools];

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no) : 
    base_frame_no(_base_frame_no), nframes(_n_frames), info_frame_no(_info_frame_no), nFreeFrames(_n_frames)
{
    assert(_n_frames <= FRAME_SIZE * 2);
    assert(npools != maxPools);

    n_info_frames = needed_info_frames(nframes);

    //allocate bitmap
    if (info_frame_no == 0) bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    else bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);

    //initialize frames to free state
    for (int fno = 0; fno < _n_frames; fno++) set_state(fno, FrameState::Free);
    
    //allocate info frames
    if (_info_frame_no == 0) {

        if (n_info_frames <= 1) {
            set_state(0, FrameState::HoS);
            nFreeFrames--;
        }
        else {
            mark_inaccessible(0, n_info_frames);
        }
    }

    //keep track of the current amount of frame pools
    pools[npools] = this;
    npools++;

    Console::puts("Frame pool initialized! \n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    assert(nFreeFrames >= _n_frames);

    unsigned int fno = base_frame_no;
    unsigned int seq_start = 0;

    //while we haven't reached the end of the bitmap ...
    while (seq_start <= nframes - _n_frames) {

        //identify the next available free frame
        while (seq_start < nframes && get_state(seq_start) != FrameState::Free) 
            seq_start++;

        unsigned int search_frame = seq_start;
        unsigned int seq_length = 0;
        //see how long the sequence goes for ...
        while (search_frame < nframes && get_state(search_frame) == FrameState::Free && seq_length < _n_frames) {
            search_frame++;
            seq_length++;
        }
        //if the sequence is big enough, allocate the space
        if (seq_length == _n_frames) {
            fno += seq_start;
            mark_inaccessible(fno, _n_frames);
            return fno;
        }

        if (search_frame >= nframes) return 0;
    }

    return 0;
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    unsigned int base_index = _base_frame_no - base_frame_no;
    assert(get_state(base_index) == FrameState::Free);

    //iterate from base to end frame, mark each as used
    set_state(base_index, FrameState::HoS);
	for (int fno = base_index + 1; fno < base_index + _n_frames; fno++) {
        assert(get_state(fno) == FrameState::Free);
        set_state(fno, FrameState::Used);
	}
	
	nFreeFrames -= _n_frames;

}

void ContFramePool::_release_frames(unsigned long _first_frame_no)
{

    set_state(_first_frame_no, FrameState::Free);
    nFreeFrames += 1;
    //iterate from start to end of sequence, free frames
    unsigned long fno = _first_frame_no + 1;
    while (fno < base_frame_no + nframes && get_state(fno) == FrameState::Used) {
        set_state(fno, FrameState::Free);
        nFreeFrames += 1;
    }
}


void ContFramePool::release_frames(unsigned long _first_frame_no)
{

    //for each frame pool ...
    for (ContFramePool* pool : pools) {
        //if the frame is within the pool's bounds
        if (_first_frame_no >= pool->base_frame_no && 
            _first_frame_no < pool->base_frame_no + pool->nframes) {
            //release the frames
            pool->_release_frames(_first_frame_no);
            break;
        }
    }

    
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
	return (_n_frames) / FRAME_SIZE + (_n_frames % FRAME_SIZE > 0 ? 1 : 0);
}

ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no) {        

    switch (bitmap[_frame_no]) {
        case 0:
            return FrameState::Used;
        case 1:
            return FrameState::Free;
        case 2:
            return FrameState::HoS;
    }

    return FrameState::Used;
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state) {

    unsigned char new_state_bits = 0;
    switch (_state) {
        case FrameState::Used:
            new_state_bits = 0;
            break;
        case FrameState::Free:
            new_state_bits = 1;
            break;
        case FrameState::HoS:
            new_state_bits = 2;
            break;
    }

    bitmap[_frame_no] = new_state_bits;
}