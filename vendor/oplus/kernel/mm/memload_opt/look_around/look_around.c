// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (C) 2020-2022 Oplus. All rights reserved.
*/
#include <linux/export.h>
#include <linux/module.h>
#include <linux/rmap.h>
#include <linux/bitops.h>
#include <linux/proc_fs.h>
#include <trace/hooks/vmscan.h>
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/debug.h>
#include <trace/hooks/cgroup.h>
#include <trace/hooks/sys.h>
#include <trace/hooks/mm.h>

#define LOOK_AROUND_MAX 8

#define PG_lookaround_ref (__NR_PAGEFLAGS + 1)
#define SetPageLookAroundRef(folio) set_bit(PG_lookaround_ref, &(folio)->flags)
#define ClearPageLookAroundRef(folio) clear_bit(PG_lookaround_ref, &(folio)->flags)
#define TestClearPageLookAroundRef(folio) test_and_clear_bit(PG_lookaround_ref, &(folio)->flags)

atomic64_t lookaround_skip_rmap  = ATOMIC_LONG_INIT(0);

enum page_references {
	PAGEREF_RECLAIM,
	PAGEREF_RECLAIM_CLEAN,
	PAGEREF_KEEP,
	PAGEREF_ACTIVATE,
};

static void page_referenced_look_around(struct page_vma_mapped_walk *pvmw)
{
	int i;
	pte_t *pte;
	unsigned long start;
	unsigned long end;
	unsigned long addr;
	unsigned long look_around_pages = LOOK_AROUND_MAX;
	struct folio *folio = pfn_folio(pvmw->pfn);
	struct pglist_data *pgdat = folio_pgdat(folio);
	unsigned long bitmap[BITS_TO_LONGS(LOOK_AROUND_MAX *2)] = {};

	lockdep_assert_held(pvmw->ptl);
	VM_WARN_ON_ONCE_FOLIO(folio_test_lru(folio), folio);

	start = max(pvmw->address & PMD_MASK, pvmw->vma->vm_start);
	end = pmd_addr_end(pvmw->address, pvmw->vma->vm_end);

	if (end - start > look_around_pages * 2 * PAGE_SIZE) {
		if (pvmw->address - start < look_around_pages * PAGE_SIZE)
			end = start + look_around_pages * 2 * PAGE_SIZE;
		else if (end - pvmw->address < look_around_pages * PAGE_SIZE)
			start = end - look_around_pages * 2 * PAGE_SIZE;
		else {
			start = pvmw->address - look_around_pages * PAGE_SIZE;
			end = pvmw->address + look_around_pages * PAGE_SIZE;
		}
	}
	pte = pvmw->pte - (pvmw->address - start) / PAGE_SIZE;

	arch_enter_lazy_mmu_mode();

	for (i = 0, addr = start; addr != end; i++, addr += PAGE_SIZE) {
		struct folio *folio;
		unsigned long pfn = pte_pfn(pte[i]);

		if (!pte_present(pte[i]) || is_zero_pfn(pfn))
			continue;

		if (WARN_ON_ONCE(pte_devmap(pte[i]) || pte_special(pte[i])))
			continue;

		if (!pte_young(pte[i]))
			continue;

		VM_BUG_ON(!pfn_valid(pfn));
		if (pfn < pgdat->node_start_pfn || pfn >= pgdat_end_pfn(pgdat))
			continue;

		folio = pfn_folio(pfn);
		if (folio_nid(folio) != pgdat->node_id)
			continue;

		VM_BUG_ON(addr < pvmw->vma->vm_start || addr >= pvmw->vma->vm_end);
		if (!ptep_test_and_clear_young(pvmw->vma, addr, pte + i))
			continue;

		if (pte_dirty(pte[i]) && !folio_test_dirty(folio) &&
		    !(folio_test_anon(folio) && folio_test_swapbacked(folio) && !folio_test_swapcache(folio)))
			__set_bit(i, bitmap);

		/*
		 * mark the neighbour pages lookaroundref so that we skip
		 * rmap in eviction accordingly
		 */
		SetPageLookAroundRef(folio);
		if (pvmw->vma->vm_flags & VM_EXEC)
			folio_set_referenced(folio);
	}

	arch_leave_lazy_mmu_mode();

	for_each_set_bit(i, bitmap, look_around_pages * 2)
		set_page_dirty(pte_page(pte[i]));
}

static void look_around_handler(void *unused, struct page_vma_mapped_walk *pvmw, struct folio *folio,
										struct vm_area_struct *vma, int *referenced)
{
	if (pte_young(*pvmw->pte) &&
		!(vma->vm_flags & (VM_SEQ_READ | VM_RAND_READ))) {
		page_referenced_look_around(pvmw);
		*referenced += 1;
	}
}

static void check_folio_look_around_ref_handler(void *unused, struct folio *folio, int *look_around_ref)
{
	/*
	* look-around has seen the page is active so we can skip the rmap
	* we can neither use PG_active nor invent a new PG_ flag, so we
	* hardcode PG_lookaround_ref by __NR_PAGEFLAGS + 1
	*/
	if (TestClearPageLookAroundRef(folio)) {
		atomic64_inc(&lookaround_skip_rmap);
		if (folio_test_referenced(folio))
			*look_around_ref = PAGEREF_ACTIVATE;
		else {
			folio_set_referenced(folio);
			*look_around_ref = PAGEREF_KEEP;
		}
	} else {
		*look_around_ref = 0;
	}
}

static void test_clear_look_around_ref_handler(void *unused, struct page *page)
{
	ClearPageLookAroundRef(page);
}

static void look_around_migrate_folio_handler(void *unused, struct folio *old_folio, struct folio *new_folio)
{
	if (TestClearPageLookAroundRef(old_folio)) {
		folio_set_referenced(new_folio);
	}
}

static int register_look_around_vendor_hooks(void)
{
	int ret = 0;
	ret = register_trace_android_vh_look_around(look_around_handler, NULL);
	if (ret != 0) {
		pr_err("register look_around vendor_hooks failed! ret=%d\n", ret);
		goto out;
	}

	ret = register_trace_android_vh_check_folio_look_around_ref(check_folio_look_around_ref_handler, NULL);
	if (ret != 0) {
		pr_err("register check_page_look_around_ref vendor_hooks failed! ret=%d\n", ret);
		goto out1;
	}

	ret = register_trace_android_vh_test_clear_look_around_ref(test_clear_look_around_ref_handler, NULL);
	if (ret != 0) {
		pr_err("register check_page_look_around_ref vendor_hooks failed! ret=%d\n", ret);
		goto out2;
	}

	ret = register_trace_android_vh_look_around_migrate_folio(look_around_migrate_folio_handler, NULL);
	if (ret != 0) {
		pr_err("register check_page_look_around_ref vendor_hooks failed! ret=%d\n", ret);
		goto out3;
	}

	return ret;

out3:
	register_trace_android_vh_test_clear_look_around_ref(test_clear_look_around_ref_handler, NULL);
out2:
	unregister_trace_android_vh_check_folio_look_around_ref(check_folio_look_around_ref_handler, NULL);
out1:
	unregister_trace_android_vh_look_around(look_around_handler, NULL);
out:
	return ret;
}

static void unregister_look_around_vendor_hooks(void)
{
	unregister_trace_android_vh_look_around(look_around_handler, NULL);
	unregister_trace_android_vh_check_folio_look_around_ref(check_folio_look_around_ref_handler, NULL);
	unregister_trace_android_vh_test_clear_look_around_ref(test_clear_look_around_ref_handler, NULL);
	unregister_trace_android_vh_look_around_migrate_folio(look_around_migrate_folio_handler, NULL);
}

static int look_around_show(struct seq_file *s, void *v)
{
	seq_printf(s, "look_around pages:  %d \n", LOOK_AROUND_MAX * 2);
	seq_printf(s, "rmap_skip:  %llu \n", atomic64_read(&lookaround_skip_rmap));

	return 0;
}

static int __init look_around_init(void)
{
	int ret = 0;

	ret = register_look_around_vendor_hooks();
	if (ret != 0)
		return ret;
	proc_create_single("look_around_stat", 0, NULL, look_around_show);
	pr_info("look_around_init succeed!\n");
	return ret;
}

static void __exit look_around_exit(void)
{
	unregister_look_around_vendor_hooks();
	pr_info("look_around exit succeed!\n");
	return;
}

module_init(look_around_init);
module_exit(look_around_exit);

MODULE_LICENSE("GPL v2");

