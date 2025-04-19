/* Implements Data pages and slotted page operations */

/* TODO:
 * Data operations:
 * 1) In reality, due to overflow pages, pulling out data is harder than it sounds. it might require the following of a linked list style overflow page. Overflow pages themselves also have chunks, i.e multiple data pages might store chunks of their data inside an overflow page. So you need to way to follow through each overflow page
 * 2) You need to deal with slotted page vaccuming - define vaccum()
 * 3) Also need to deal with storing an overflow data type
*/
