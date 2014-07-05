#include "ClientGroup.h"

Map<xcb_window_t, ClientGroup::WeakPtr> ClientGroup::sGroups;
