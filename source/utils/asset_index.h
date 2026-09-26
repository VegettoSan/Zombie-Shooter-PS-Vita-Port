#ifndef ZOMBIE_ASSET_INDEX_H
#define ZOMBIE_ASSET_INDEX_H
/* Only hot, read-only packaged asset directories are indexed. A complete
 * listing proves absence; all uncertain/unsupported cases use the old open. */
int asset_index_missing(const char *filename);
void asset_index_report(void);
#endif
