/*
 * Buddy Page Allocation Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
 */

/*
 * STUDENT NUMBER: s1770036
 */
#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

#define MAX_ORDER	17

/**
 * A buddy page allocation algorithm.
 */
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
private:
	/**
	 * Returns the number of pages that comprise a 'block', in a given order.
	 * @param order The order to base the calculation off of.
	 * @return Returns the number of pages in a block, in the order.
	 */
	static inline constexpr uint64_t pages_per_block(int order)
	{
		/* The number of pages per block in a given order is simply 1, shifted left by the order number.
		 * For example, in order-2, there are (1 << 2) == 4 pages in each block.
		 */
		return (1 << order);
	}

	/**
	 * Returns TRUE if the supplied page descriptor is correctly aligned for the
	 * given order.  Returns FALSE otherwise.
	 * @param pgd The page descriptor to test alignment for.
	 * @param order The order to use for calculations.
	 */
	static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
	{
		// Calculate the page-frame-number for the page descriptor, and return TRUE if
		// it divides evenly into the number pages in a block of the given order.
		return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
	}

	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	 * to the left or the right of PGD, in the given order.
	 * @param pgd The page descriptor to find the buddy for.
	 * @param order The order in which the page descriptor lives.
	 * @return Returns the buddy of the given page descriptor, in the given order.
	 */
	PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
	{
		// (1) Make sure 'order' is within range
		if (order >= MAX_ORDER) {
			return NULL;
		}

		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order)) {
			return NULL;
		}

		// (3) Calculate the page-frame-number of the buddy of this page.
		// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
		// * If it's not aligned, then the buddy must be the previous block in THIS order.
		uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ?
			sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) :
			sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);

		// (4) Return the page descriptor associated with the buddy page-frame-number.
		return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
	}

	/**
	 * Inserts a block into the free list of the given order.  The block is inserted in ascending order.
	 * @param pgd The page descriptor of the block to insert.
	 * @param order The order in which to insert the block.
	 * @return Returns the slot (i.e. a pointer to the pointer that points to the block) that the block
	 * was inserted into.
	 */
	PageDescriptor **insert_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, find the slot in which the page descriptor
		// should be inserted.
		PageDescriptor **slot = &_free_areas[order];

		// Iterate whilst there is a slot, and whilst the page descriptor pointer is numerically
		// greater than what the slot is pointing to.
		while (*slot && pgd > *slot) {
			slot = &(*slot)->next_free;
		}

		// Insert the page descriptor into the linked list.
		pgd->next_free = *slot;
		*slot = pgd;

		// Return the insert point (i.e. slot)
		return slot;
	}

	/**
	 * Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
	 * the system will panic.
	 * @param pgd The page descriptor of the block to remove.
	 * @param order The order in which to remove the block from.
	 */
	void remove_block(PageDescriptor *pgd, int order)
	{

		// Starting from the _free_area array, iterate until the block has been located in the linked-list.
		PageDescriptor **slot = &_free_areas[order];
		while (*slot && pgd != *slot) {
			slot = &(*slot)->next_free;

		}

		// Make sure the block actually exists.  Panic the system if it does not.
		assert(*slot == pgd);

		// Remove the block from the free list.
		*slot = pgd->next_free;
		pgd->next_free = NULL;
	}

	/**
	 * Given a pointer to a block of free memory in the order "source_order", this function will
	 * split the block in half, and insert it into the order below.
	 * @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	 * @param source_order The order in which the block of free memory exists.  Naturally,
	 * the split will insert the two new blocks into the order below.
	 * @return Returns the left-hand-side of the new block.
	 */
	PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order)
	{
		// Make sure there is an incoming pointer.
		assert(*block_pointer);

		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

		assert(source_order>0 && source_order<MAX_ORDER);

		// remove block from current order
		remove_block(*block_pointer, source_order);

		//insert 2 blocks at lower order
		insert_block((*block_pointer), source_order-1);
		insert_block(*block_pointer+pages_per_block(source_order-1), source_order-1);


		return *block_pointer;

	}

	/**
	 * Takes a block in the given source order, and merges it (and it's buddy) into the next order.
	 * This function assumes both the source block and the buddy block are in the free list for the
	 * source order.  If they aren't this function will panic the system.
	 * @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	 * @param source_order The order in which the pair of blocks live.
	 * @return Returns the new slot that points to the merged block.
	 */
	PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order) //merge_block(&_free_areas[up_id])
	{

		assert(*block_pointer);

		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

		//remove the source and its buddy
		remove_block(buddy_of(*block_pointer, source_order), source_order);
		remove_block((*block_pointer), source_order);

		//insert up whichever lower page, onto a higher order
		if ((*block_pointer)<buddy_of(*block_pointer, source_order)) {
			return insert_block(*block_pointer, source_order+1);
		}

		else {
			return insert_block(buddy_of(*block_pointer, source_order), source_order+1);
		}


	}

public:
	/**
	 * Constructs a new instance of the Buddy Page Allocator.
	 */
	BuddyPageAllocator() {
		// Iterate over each free area, and clear it.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			_free_areas[i] = NULL;
		}
	}

	/**
	 * Allocates 2^order number of contiguous pages
	 * @param order The power of two, of the number of contiguous pages to allocate.
	 * @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	 * allocation failed.
	 */
	PageDescriptor *alloc_pages(int order) override
	{
		//make sure order is within limits
		assert(order >= 0 && order < MAX_ORDER);


		int found_idx = order;

		// ascend the order list to find a free area
		while ((found_idx < MAX_ORDER-1) && (_free_areas[found_idx] == NULL)) {
			found_idx++;
		}

		// return NULL if can't find a free area
		if (found_idx >= MAX_ORDER) {
			return NULL;
		}

		// found free area
		PageDescriptor *pgd = _free_areas[found_idx];


		// keep on splitting until find desired order
		while (found_idx != order) {
			pgd = split_block(&pgd, found_idx);

			found_idx--;
		}

		// dont split this pgd, remove it and return it
		remove_block(pgd, order);

		return pgd;

	}

	/**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
	void free_pages(PageDescriptor *pgd, int order) override
	{

		// Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		assert(is_correct_alignment_for_order(pgd, order));

		//free all the pages here.
		insert_block(pgd, order);

		//start iterating from desired order
		int up_id = order;

		// go up the list
		while (up_id < MAX_ORDER-1) {

			//find buddy of this page
			PageDescriptor **slot = &_free_areas[up_id];

			while (*slot && (buddy_of(pgd, up_id) != *slot)) {
				slot = &(*slot)->next_free;
			}

			//buddy found, merge
			if (buddy_of(pgd, up_id) == *slot) {
				pgd = *merge_block(&pgd, up_id);
				up_id ++;
				// mm_log.messagef(LogLevel::DEBUG, "successfully merged!: ---------- %d", order);
			}
			//else next order
			else {
				break;
				// mm_log.messagef(LogLevel::DEBUG, "DID NOT merged!: ---------- %d", order);
			}
		}

	}


	/**
	 * Reserves a specific page, so that it cannot be allocated.
	 * @param pgd The page descriptor of the page to reserve.
	 * @return Returns TRUE if the reservation was successful, FALSE otherwise.
	 */

	bool reserve_page(PageDescriptor *pgd)
	{
			// find the closest order with a free area
			int found_idx = 0;
			while ((found_idx < MAX_ORDER-1) && (NULL == _free_areas[found_idx])) {
				found_idx++;
			}

			// if no spaces found return false
			if (found_idx >= MAX_ORDER) {
				return false;
			}

			// found a free area
			PageDescriptor **slot = &_free_areas[found_idx];
			PageDescriptor *right_child;


			while (*slot && (pgd >= *slot)) {

				// if the requested page is in the range of this block, split and search this block
				if (pgd < (*slot)->next_free) {

							// while the block hasn't been found. keep on splitting
							while (pgd != *slot && found_idx>0) {

									// split and obtain left, right child blocks at lower order
									*slot = split_block(slot, found_idx);

									// make sure buddy of this slot is in free list
									PageDescriptor **search_buddy = slot;
									while (*search_buddy && (buddy_of(*slot, found_idx-1) > *slot)) {
										search_buddy = &(*search_buddy)->next_free;

										if(buddy_of(*slot, found_idx-1) == *search_buddy) {
											right_child = buddy_of(*slot, found_idx-1);
											break;
										}
									}

									// if pgd is less than the right child, split and search left child
									if (pgd < right_child) {
										found_idx--;

									}
									// else split and search the right child
									else if (pgd >= right_child) {
										*slot = right_child;
										found_idx--;
									}

							}

							break;
				}

				slot = &(*slot)->next_free;


		}

	remove_block(*slot, found_idx);
	return true;
	}


	/**
	 * Initialises the allocation algorithm.
	 * @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	 */
	bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override
	{
		mm_log.messagef(LogLevel::DEBUG, "Buddy Allocator Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors); //0xff..80147000

		// if there are no pages to be allocated, fail to initialise
		if (nr_page_descriptors == 0) {
			return false;
		}

		// only the highest order contains blocks
		for (uint64_t i = 0; i < MAX_ORDER-1; i++) {
			_free_areas[i] = NULL;
		}

		_free_areas[MAX_ORDER-1] = &page_descriptors[0]; //17th order has the address of the first physical page
		PageDescriptor **slot = &_free_areas[MAX_ORDER-1];


		// only if total # physical pages is divisible by the biggest block
		if (( nr_page_descriptors % pages_per_block(MAX_ORDER-1)) == 0) {

			for (uint64_t i = pages_per_block(MAX_ORDER-1); i < nr_page_descriptors; i+=pages_per_block(MAX_ORDER-1)) {
				(*slot)->next_free = &page_descriptors[i];
				slot = &(*slot)->next_free;

			}
		}
		return true;
}



	/**
	 * Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "buddy"; }

	/**
	 * Dumps out the current state of the buddy system
	 */
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");

		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);

			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg) {
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}

			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
		}
	}


private:
	PageDescriptor *_free_areas[MAX_ORDER]; //there are 18 items, from orders 0-17

};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
 * Allocation algorithm registration framework
 */
RegisterPageAllocator(BuddyPageAllocator);
