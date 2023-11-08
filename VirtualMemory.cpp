#include "VirtualMemory.h"
#include "PhysicalMemory.h"

#define FIRST_FRAME_OFFSET (VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH * TABLES_DEPTH)
#define FIRST_FRAME_NUM_ROW ((1LL) << (FIRST_FRAME_OFFSET))
#define SUCCESS 1
#define FAILURE 0

typedef uint64_t FrameType;
typedef word_t ContentType;

enum State{
    READ = 1,
    WRITE = 0
};

typedef struct parent_frame {
    FrameType frame = 0;
    FrameType index = 0;
} parent_frame;

typedef struct cyclic_max_frame {
    FrameType max_dist = 0;
    FrameType page = 0;
    FrameType frame = 0;
    parent_frame parent;
} cyclic_max_frame;



FrameType cyclic_dist(FrameType page_target, FrameType page_number) {

    FrameType calc = (page_number > page_target) ? (page_number - page_target) : (page_target - page_number);
    ///Calculates the min distance according to the equation given in the exercise */
    if (NUM_PAGES - calc < calc) {
        return NUM_PAGES - calc;
    } else {
        return calc;
    }
}


FrameType offset_calculation(FrameType virtualAddress, int depth) {
    ///Returns the correct offset for a specific depth of tree*/
    if (depth == 0) {
        return virtualAddress >> (OFFSET_WIDTH * TABLES_DEPTH);
    }
    uint64_t x = 0LL;
    for (int i = 0; i < OFFSET_WIDTH; ++i) {
        x = x + (1LL << (((VIRTUAL_ADDRESS_WIDTH)-((FIRST_FRAME_OFFSET)+(depth*(OFFSET_WIDTH))))+i));
    }
    FrameType offset = virtualAddress&x;
    offset = offset >> ((VIRTUAL_ADDRESS_WIDTH)-((FIRST_FRAME_OFFSET)+(depth*(OFFSET_WIDTH))));
    return offset;

}


int check_empty_table(FrameType frame, int num_rows) {
    int check = 0;
    for (int i = 0; i < num_rows; ++i) {
        FrameType address = frame * PAGE_SIZE + i;
        ContentType content;
        PMread(address, &content);
        if (content == 0) {
            check += 1;
        }
    }
    if (check == num_rows) {
        return 1;
    }
    return 0;
}


void detach_ref(parent_frame *parent_frame) {
    /// Detach the reference from the parent frame to the child frame */
    FrameType address = (parent_frame->frame) * PAGE_SIZE + (parent_frame->index);
    PMwrite(address, 0);
}


ContentType DFS_traversal_helper(ContentType frame, parent_frame *parent, ContentType *max_frame_number, int depth, FrameType target,
                               cyclic_max_frame *cur_cyc, ContentType illigal_frame, FrameType page_number) {
    /// check if current address is the max frame number
    if (frame > (*max_frame_number)) {
        (*max_frame_number) = frame;
    }

    /// if the frame is a leaf - calculate the cyclic distance and exit the function.
    if (depth == TABLES_DEPTH) {
        if (parent == nullptr) {
            if (check_empty_table(frame, FIRST_FRAME_NUM_ROW)) {
                return frame;
            }
        }

        auto distance = cyclic_dist(target, page_number);
        if (distance > cur_cyc->max_dist) {
            cur_cyc->max_dist = distance;
            cur_cyc->page = page_number;
            cur_cyc->frame = frame;
            cur_cyc->parent = *parent;
        }
        return 0;
    }

    /// if we are not a leaf - check if the frame is empty - yes: detach parent link to that frame.
    int num_rows = (frame == 0) ? FIRST_FRAME_NUM_ROW : PAGE_SIZE;
    int num_zeros = 0;
    for (int i = 0; i < PAGE_SIZE; ++i) {
        FrameType address = frame * PAGE_SIZE + i;
        ContentType row_content =0;
        PMread(address, &row_content);
        if (row_content == 0) {
            ++num_zeros;
            continue;
        } else {
            parent->index = i;
            parent->frame = frame;
            FrameType new_page_num = (page_number << OFFSET_WIDTH) + i;
            auto res = DFS_traversal_helper(row_content, parent, max_frame_number,
                                            depth + 1, target, cur_cyc, illigal_frame, new_page_num);
            if (res != 0) {
                if (res == illigal_frame)
                {
                    continue;
                }
                else{
                    return res;
                }
            }
        }
    }
    /// if all entries are zeros then detach link from parent frame
    if (num_zeros == num_rows){
        if (frame != 0 && frame != illigal_frame) { /// the root frame doesn't have a parent frame
            detach_ref(parent);
        }
        return frame;
    }
    return 0;
}


ContentType DFS_traversal(FrameType target, ContentType illigal_frame, cyclic_max_frame* cur_cyc,parent_frame* parent) {
    /// Traverses the tree in DFS manner and returns the first empty frame that is not referenced by any other frame
    ContentType max_frame_number = 0;
    auto res = DFS_traversal_helper(0, parent, &max_frame_number, 0, target, cur_cyc, illigal_frame,0);
    if (res == 0) {
        if (max_frame_number < NUM_FRAMES -1 ) {
            return max_frame_number + 1;
        } else {
            return 0;
        }
    }
    return res;
}


FrameType evict_cyclic_frame(cyclic_max_frame* cmf)
{
    /// Evicts the frame with the max cyclic distance
    if(cmf->max_dist == 0)
    {
        return 0;
    }
    PMevict(cmf->frame, cmf->page);
    detach_ref(&(cmf->parent));
    return cmf->frame;

}


void zero_the_frame(FrameType frame, int depth){
    ///zero the new frame
    if(depth < TABLES_DEPTH-1) {
        for (int i = 0; i < PAGE_SIZE; ++i) {
            PMwrite(frame * PAGE_SIZE + i, 0);
        }
    }
}


FrameType find_free_frame(ContentType illigal_frame, FrameType page_number, int depth) {
    /// Finds a free frame in the tree and returns it's number
    parent_frame parent;
    cyclic_max_frame cmf;
    auto res = DFS_traversal(page_number, illigal_frame, &cmf, &parent);
    if (res == 0) {
        auto frame = evict_cyclic_frame(&cmf);
        zero_the_frame(frame, depth);
        return frame;
    }
    ///zero the new frame
    zero_the_frame(res, depth);
    return res;
}


int VM_helper(FrameType virtualAddress, ContentType* value, int depth, ContentType curr_table_address, State state,
              bool is_new_frame) {
    if (depth == TABLES_DEPTH ) {
        /// We have reached the leaf
        auto offset_index = offset_calculation(virtualAddress, depth);
        if(is_new_frame)
        {
            PMrestore((uint64_t)curr_table_address ,virtualAddress>>OFFSET_WIDTH);
        }
        if(state == READ)
        {
            PMread(curr_table_address * PAGE_SIZE + offset_index, value);
        }else if(state == WRITE)
        {
            PMwrite(curr_table_address * PAGE_SIZE + offset_index, *value);
        }
        return SUCCESS;
    }
    auto old_frame = curr_table_address;
    /// extract the offset from the virtual frame
    auto offset_index = offset_calculation(virtualAddress, depth);
    PMread(old_frame * PAGE_SIZE + offset_index, &curr_table_address);

    /// find an empty frame
    if(curr_table_address == 0){
        auto frame = find_free_frame(old_frame, virtualAddress >> OFFSET_WIDTH,depth);
        PMwrite(old_frame * PAGE_SIZE + offset_index , (ContentType)frame);
        is_new_frame = true;
        curr_table_address = (ContentType)frame;
        return VM_helper(virtualAddress, value, depth + 1, curr_table_address, state, is_new_frame);
    }
    /// we found a frame - continue the search
    is_new_frame = false;
    return VM_helper(virtualAddress, value, depth + 1, curr_table_address, state, is_new_frame);
}

/*
 * Initialize the virtual memory.
 */
void VMinitialize() {
    /// initialize the first frame to zeros
    for (FrameType i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(i, 0);
    }
}

/* Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(FrameType virtualAddress, ContentType *value) {
    if(virtualAddress >= VIRTUAL_MEMORY_SIZE || value == nullptr)
    {
        return FAILURE;
    }

    return VM_helper(virtualAddress, value, 0,0, READ, false);

}

/* Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(FrameType virtualAddress, ContentType value){
    if(virtualAddress >= VIRTUAL_MEMORY_SIZE)
    {
        return FAILURE;
    }
    return VM_helper(virtualAddress, &value, 0,0, WRITE, false);

}
