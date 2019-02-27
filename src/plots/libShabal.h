#include <cstdarg>
#include <cstdint>
#include <cstdlib>

extern "C" {

extern void find_best_deadline_sph(const char *scoops,
                                   uint64_t nonce_count,
                                   const char *gensig,
                                   uint64_t *best_deadline,
                                   uint64_t *best_offset);

uint64_t shabal_findBestDeadline(const char *scoops, uint64_t nonce_count, const char *gensig);

void shabal_findBestDeadlineDirect(const char *scoops,
                                   uint64_t nonce_count,
                                   const char *gensig,
                                   uint64_t *best_deadline,
                                   uint64_t *best_offset);

void shabal_init();

} // extern "C"
