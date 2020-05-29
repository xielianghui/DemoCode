#include <atomic>
#include <assert.h>

/**
 * 一个基于CAS的无锁链表，值得注意的是ABA问题的解决方法：对于每个节点增加一个原子的引用计数。
 * 假如线程A刚刚运行完第58行，获取到了头指针和头指针的next指针副本。然后线程B大喊一声:"咋瓦鲁多",
 * 线程A这时候卡住不动，线程B一路畅通无阻的取出表头。但是因为线程A和B都对引用计数做了fetch_add()，
 * 所以线程B手里的头节点引用计数是1。如果线程B想要把这个表头还插入回去，引起ABA问题是没法实现的。因为
 * 要么线程A做CAS发现不对后把引用计数减成0，然后线程B自己插入，要么就是引用计数为1直接返回，然后线程A
 * 帮线程B插入(73行)。
 */
template <typename N>
struct FreeListNode
{
    FreeListNode() : freeListRefs(0), freeListNext(nullptr) { }

    std::atomic<std::uint32_t> freeListRefs;
    std::atomic<N*> freeListNext;
};

// A simple CAS-based lock-free free list. Not the fastest thing in the world under heavy contention, but
// simple and correct (assuming nodes are never freed until after the free list is destroyed), and fairly
// speedy under low contention.
template<typename N>		// N must inherit FreeListNode or have the same fields (and initialization of them)
struct FreeList
{
    FreeList() : freeListHead(nullptr) { }
    FreeList(FreeList&& other) : freeListHead(other.freeListHead.load(std::memory_order_relaxed)) { other.freeListHead.store(nullptr, std::memory_order_relaxed); }
    void swap(FreeList& other) { details::swap_relaxed(freeListHead, other.freeListHead); }

    FreeList(FreeList const&) = delete;
    FreeList& operator=(FreeList const&) = delete;

    inline void add(N* node)
    {	
        // We know that the should-be-on-freelist bit is 0 at this point, so it's safe to
        // set it using a fetch_add
        if (node->freeListRefs.fetch_add(SHOULD_BE_ON_FREELIST, std::memory_order_acq_rel) == 0) {
            // Oh look! We were the last ones referencing this node, and we know
            // we want to add it to the free list, so let's do it!
            add_knowing_refcount_is_zero(node);
        }
    }

    inline N* try_get()
    {	
        auto head = freeListHead.load(std::memory_order_acquire);
        while (head != nullptr) {
            auto prevHead = head;
            auto refs = head->freeListRefs.load(std::memory_order_relaxed);
            if ((refs & REFS_MASK) == 0 || !head->freeListRefs.compare_exchange_strong(refs, refs + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                head = freeListHead.load(std::memory_order_acquire);
                continue;
            }

            // Good, reference count has been incremented (it wasn't at zero), which means we can read the
            // next and not worry about it changing between now and the time we do the CAS
            auto next = head->freeListNext.load(std::memory_order_relaxed);
            if (freeListHead.compare_exchange_strong(head, next, std::memory_order_acquire, std::memory_order_relaxed)) {
                // Yay, got the node. This means it was on the list, which means shouldBeOnFreeList must be false no
                // matter the refcount (because nobody else knows it's been taken off yet, it can't have been put back on).
                assert((head->freeListRefs.load(std::memory_order_relaxed) & SHOULD_BE_ON_FREELIST) == 0);

                // Decrease refcount twice, once for our ref, and once for the list's ref
                head->freeListRefs.fetch_sub(2, std::memory_order_release);
                return head;
            }

            // OK, the head must have changed on us, but we still need to decrease the refcount we increased.
            // Note that we don't need to release any memory effects, but we do need to ensure that the reference
            // count decrement happens-after the CAS on the head.
            refs = prevHead->freeListRefs.fetch_sub(1, std::memory_order_acq_rel);
            if (refs == SHOULD_BE_ON_FREELIST + 1) {
                add_knowing_refcount_is_zero(prevHead);
            }
        }

        return nullptr;
    }

    // Useful for traversing the list when there's no contention (e.g. to destroy remaining nodes)
    N* head_unsafe() const { return freeListHead.load(std::memory_order_relaxed); }

private:
    inline void add_knowing_refcount_is_zero(N* node)
    {
        // Since the refcount is zero, and nobody can increase it once it's zero (except us, and we run
        // only one copy of this method per node at a time, i.e. the single thread case), then we know
        // we can safely change the next pointer of the node; however, once the refcount is back above
        // zero, then other threads could increase it (happens under heavy contention, when the refcount
        // goes to zero in between a load and a refcount increment of a node in try_get, then back up to
        // something non-zero, then the refcount increment is done by the other thread) -- so, if the CAS
        // to add the node to the actual list fails, decrease the refcount and leave the add operation to
        // the next thread who puts the refcount back at zero (which could be us, hence the loop).
        auto head = freeListHead.load(std::memory_order_relaxed); // 
        while (true) {
            node->freeListNext.store(head, std::memory_order_relaxed);
            node->freeListRefs.store(1, std::memory_order_release);
            if (!freeListHead.compare_exchange_strong(head, node, std::memory_order_release, std::memory_order_relaxed)) {
                // Hmm, the add failed, but we can only try again when the refcount goes back to zero
                if (node->freeListRefs.fetch_add(SHOULD_BE_ON_FREELIST - 1, std::memory_order_release) == 1) {
                    continue;
                }
            }
            return;
        }
    }

private:
    // Implemented like a stack, but where node order doesn't matter (nodes are inserted out of order under contention)
    std::atomic<N*> freeListHead;

    static const std::uint32_t REFS_MASK = 0x7FFFFFFF;            //2147483647
    static const std::uint32_t SHOULD_BE_ON_FREELIST = 0x80000000;//2147483648
};