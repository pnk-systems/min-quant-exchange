// Order's member functions are small enough to live inline in the header
// (include/miniq/order.hpp). This translation unit exists to give Order
// a stable .cpp home per the project's file layout and as the natural
// place to add out-of-line members (e.g. a future toString(Order)) without
// bloating the header. Kept intentionally minimal — see the header for
// the full class definition and Rule-of-Zero rationale.
#include "miniq/order.hpp"
